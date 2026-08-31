
#ifndef __QNX__
#error "Something wrong"
#endif

#include <stdint.h>

#include <sys/syspage.h>
#include <sys/auxv.h>
#include <arm_sve.h>

#include "xbyak_aarch64_err.h"
#include "xbyak_aarch64_util.h"

namespace Xbyak_aarch64 {
namespace util{
#define L1_MAX_CORES 128
#define MAX_CORES 512

extern auxv_t* _auxv;

class CpuInfoQNX : public CpuInfo {
public:
  CpuInfoQNX() {
    init();
    cacheInfo_.midr_el1 = SYSPAGE_ENTRY(cpuinfo)->cpu;
    setNumCores();
    setHwCap();
    setCacheHierarchy();
    setImplementer();
  }

private:
  void setCacheHierarchy() {
    uint16_t num_cores = _syspage_ptr->num_cpu;
    struct cacheattr_entry *caches = SYSPAGE_ENTRY(cacheattr);
    struct cacheattr_entry *cache = &caches[0];
    uint32_t type = 1;
    while (cache->next != CACHE_LIST_END) {
      if (cache->flags == CACHE_FLAG_INSTR || cache->flags == CACHE_FLAG_DATA) {
        cacheInfo_.levelCache[type-1].type = SeparateCache;
        cacheInfo_.levelCache[type-1].size[0] = cacheInfo_.levelCache[type-1].size[1] = cache->line_size * cache->num_lines;
        cacheInfo_.levelCache[type-1].size[0] = cacheInfo_.levelCache[type-1].size[1] = 1;
      }
      else {
        cacheInfo_.levelCache[type-1].type = UnifiedCache;
        cacheInfo_.levelCache[type-1].size[2] = cache->line_size * cache->num_lines;
      }
      cache = &caches[cache->next];
      ++type;
    }
    lastDataCacheLevel_ = type;

    uint8_t l1_index[L1_MAX_CORES] = {0};
    uint32_t l2_index[MAX_CORES] = {0}, l3_index[MAX_CORES] = {0};
    struct cpuinfo_entry *cpus = SYSPAGE_ENTRY(cpuinfo);

    for (int idx = 0; idx < num_cores; idx++) {
      uint8_t data_cache = cpus[idx].data_cache;
      l1_index[data_cache]++;
    }

    for (int idx = 0; idx < L1_MAX_CORES; idx++) {
      if (l1_index[idx] != 0) {
        uint8_t data_cache_idx = idx;
        cache = &caches[data_cache_idx];
        uint32_t l2_cache_idx = cache->next;
        l2_index[l2_cache_idx]++;
        cache = &caches[l2_cache_idx];
        uint32_t l3_cache_idx = cache->next;
        l3_index[l3_cache_idx]++;
      }
    }

    for (int idx = 0; idx < MAX_CORES; idx++) {
      if (cacheInfo_.levelCache[1].sharingCores[2] == 0 && l2_index[idx] != 0) {
        cacheInfo_.levelCache[1].sharingCores[2] = l2_index[idx];
      }
      if (cacheInfo_.levelCache[2].sharingCores[2] == 0 && l3_index[idx] != 0) {
        cacheInfo_.levelCache[2].sharingCores[2] = l3_index[idx];
      }
    }
  }

  void setHwCap() {
    struct cpuinfo_entry *cpu = SYSPAGE_ENTRY(cpuinfo);
    uint64_t flags = cpu->flags;

    if (flags & AARCH64_CPU_FLAG_LSE)
      type_ |= (Type)XBYAK_AARCH64_HWCAP_ATOMIC;
    if (flags & CPU_FLAG_FPU)
      type_ |= (Type)XBYAK_AARCH64_HWCAP_FP;
    if (flags & AARCH64_CPU_FLAG_SIMD)
      type_ |= (Type)XBYAK_AARCH64_HWCAP_ADVSIMD;
    if (flags & AARCH64_CPU_FLAG_SVE) {
      type_ |= (Type)XBYAK_AARCH64_HWCAP_SVE;
      sveLen_ = svcntb() * 8;
    }
  }

  void setNumCores() {
    uint16_t num_cores = _syspage_ptr->num_cpu;
    struct cpuinfo_entry *cpus = SYSPAGE_ENTRY(cpuinfo);
    numCores_[0] = num_cores;
    uint8_t physical_cores[128] = {0};
    for (int idx = 0; idx < num_cores; idx++) {
      uint8_t hwid = static_cast<uint8_t>(cpus[idx].smp_hwcoreid);
      physical_cores[hwid]++;
    }
    numCores_[1] = 0;
    for(int idx = 0; idx < num_cores; idx++) {
      if (physical_cores[idx] != 0)
        numCores_[1]++;
    }
  }
};

} // namespace util
} // namespace Xbyak_aarch64
