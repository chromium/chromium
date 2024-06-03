// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/cpu.h"

#include <algorithm>
#include <cinttypes>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <utility>

#include "partition_alloc/build_config.h"

#if PA_BUILDFLAG(PA_ARCH_CPU_ARM_FAMILY) &&                \
    (PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX) || \
     PA_BUILDFLAG(IS_CHROMEOS))
#include <asm/hwcap.h>
#include <sys/auxv.h>

// Temporary definitions until a new hwcap.h is pulled in everywhere.
// https://crbug.com/1265965
#if PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
#ifndef HWCAP2_MTE
#define HWCAP2_MTE (1 << 18)
#endif
#ifndef HWCAP2_BTI
#define HWCAP2_BTI (1 << 17)
#endif
#endif  // PA_BUILDFLAG(PA_ARCH_CPU_ARM64)

#endif  // PA_BUILDFLAG(PA_ARCH_CPU_ARM_FAMILY) && (PA_BUILDFLAG(IS_ANDROID) ||
        // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS))

#if PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY)
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
#include <immintrin.h>  // For _xgetbv()
#include <intrin.h>
#endif
#endif

namespace partition_alloc::internal::base {

CPU::CPU() {
  Initialize();
}
CPU::CPU(CPU&&) = default;

namespace {

#if PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY)
#if !PA_BUILDFLAG(PA_COMPILER_MSVC)

#if defined(__pic__) && defined(__i386__)

void __cpuid(int cpu_info[4], int info_type) {
  __asm__ volatile(
      "mov %%ebx, %%edi\n"
      "cpuid\n"
      "xchg %%edi, %%ebx\n"
      : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]),
        "=d"(cpu_info[3])
      : "a"(info_type), "c"(0));
}

#else

void __cpuid(int cpu_info[4], int info_type) {
  __asm__ volatile("cpuid\n"
                   : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                     "=d"(cpu_info[3])
                   : "a"(info_type), "c"(0));
}

#endif
#endif  // !PA_BUILDFLAG(PA_COMPILER_MSVC)

// xgetbv returns the value of an Intel Extended Control Register (XCR).
// Currently only XCR0 is defined by Intel so |xcr| should always be zero.
uint64_t xgetbv(uint32_t xcr) {
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
  return _xgetbv(xcr);
#else
  uint32_t eax, edx;

  __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return (static_cast<uint64_t>(edx) << 32) | eax;
#endif  // PA_BUILDFLAG(PA_COMPILER_MSVC)
}

#endif  // ARCH_CPU_X86_FAMILY

}  // namespace

void CPU::Initialize() {
#if PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY)
  int cpu_info[4] = {-1};

  // __cpuid with an InfoType argument of 0 returns the number of
  // valid Ids in CPUInfo[0] and the CPU identification string in
  // the other three array elements. The CPU identification string is
  // not in linear order. The code below arranges the information
  // in a human readable form. The human readable order is CPUInfo[1] |
  // CPUInfo[3] | CPUInfo[2]. CPUInfo[2] and CPUInfo[3] are swapped
  // before using memcpy() to copy these three array elements to |cpu_string|.
  __cpuid(cpu_info, 0);
  int num_ids = cpu_info[0];
  std::swap(cpu_info[2], cpu_info[3]);

  // Interpret CPU feature information.
  if (num_ids > 0) {
    int cpu_info7[4] = {0};
    __cpuid(cpu_info, 1);
    if (num_ids >= 7) {
      __cpuid(cpu_info7, 7);
    }
    signature_ = cpu_info[0];
    stepping_ = cpu_info[0] & 0xf;
    type_ = (cpu_info[0] >> 12) & 0x3;
    has_mmx_ = (cpu_info[3] & 0x00800000) != 0;
    has_sse_ = (cpu_info[3] & 0x02000000) != 0;
    has_sse2_ = (cpu_info[3] & 0x04000000) != 0;
    has_sse3_ = (cpu_info[2] & 0x00000001) != 0;
    has_ssse3_ = (cpu_info[2] & 0x00000200) != 0;
    has_sse41_ = (cpu_info[2] & 0x00080000) != 0;
    has_sse42_ = (cpu_info[2] & 0x00100000) != 0;
    has_popcnt_ = (cpu_info[2] & 0x00800000) != 0;

    // "Hypervisor Present Bit: Bit 31 of ECX of CPUID leaf 0x1."
    // See https://lwn.net/Articles/301888/
    // This is checking for any hypervisor. Hypervisors may choose not to
    // announce themselves. Hypervisors trap CPUID and sometimes return
    // different results to underlying hardware.
    is_running_in_vm_ = (cpu_info[2] & 0x80000000) != 0;

    // AVX instructions will generate an illegal instruction exception unless
    //   a) they are supported by the CPU,
    //   b) XSAVE is supported by the CPU and
    //   c) XSAVE is enabled by the kernel.
    // See http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled
    //
    // In addition, we have observed some crashes with the xgetbv instruction
    // even after following Intel's example code. (See crbug.com/375968.)
    // Because of that, we also test the XSAVE bit because its description in
    // the CPUID documentation suggests that it signals xgetbv support.
    has_avx_ = (cpu_info[2] & 0x10000000) != 0 &&
               (cpu_info[2] & 0x04000000) != 0 /* XSAVE */ &&
               (cpu_info[2] & 0x08000000) != 0 /* OSXSAVE */ &&
               (xgetbv(0) & 6) == 6 /* XSAVE enabled by kernel */;
    has_aesni_ = (cpu_info[2] & 0x02000000) != 0;
    has_fma3_ = (cpu_info[2] & 0x00001000) != 0;
    has_avx2_ = has_avx_ && (cpu_info7[1] & 0x00000020) != 0;

    has_pku_ = (cpu_info7[2] & 0x00000010) != 0;
  }

  // Get the brand string of the cpu.
  __cpuid(cpu_info, 0x80000000);
  const int max_parameter = cpu_info[0];

  static constexpr int kParameterContainingNonStopTimeStampCounter = 0x80000007;
  if (max_parameter >= kParameterContainingNonStopTimeStampCounter) {
    __cpuid(cpu_info, kParameterContainingNonStopTimeStampCounter);
    has_non_stop_time_stamp_counter_ = (cpu_info[3] & (1 << 8)) != 0;
  }

  if (!has_non_stop_time_stamp_counter_ && is_running_in_vm_) {
    int cpu_info_hv[4] = {};
    __cpuid(cpu_info_hv, 0x40000000);
    if (cpu_info_hv[1] == 0x7263694D &&  // Micr
        cpu_info_hv[2] == 0x666F736F &&  // osof
        cpu_info_hv[3] == 0x76482074) {  // t Hv
      // If CPUID says we have a variant TSC and a hypervisor has identified
      // itself and the hypervisor says it is Microsoft Hyper-V, then treat
      // TSC as invariant.
      //
      // Microsoft Hyper-V hypervisor reports variant TSC as there are some
      // scenarios (eg. VM live migration) where the TSC is variant, but for
      // our purposes we can treat it as invariant.
      has_non_stop_time_stamp_counter_ = true;
    }
  }
#elif PA_BUILDFLAG(PA_ARCH_CPU_ARM_FAMILY)
#if PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX) || \
    PA_BUILDFLAG(IS_CHROMEOS)

#if PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
  // Check for Armv8.5-A BTI/MTE support, exposed via HWCAP2
  unsigned long hwcap2 = getauxval(AT_HWCAP2);
  has_mte_ = hwcap2 & HWCAP2_MTE;
  has_bti_ = hwcap2 & HWCAP2_BTI;
#endif

#elif PA_BUILDFLAG(IS_WIN)
  // Windows makes high-resolution thread timing information available in
  // user-space.
  has_non_stop_time_stamp_counter_ = true;
#endif
#endif
}

const CPU& CPU::GetInstanceNoAllocation() {
  static const CPU cpu;
  return cpu;
}

}  // namespace partition_alloc::internal::base
