// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc {

// Tests whether we can run extended instructions represented by the CPU
// information. This test actually executes some extended instructions (such as
// MMX, SSE, etc.) supported by the CPU and sees we can run them without
// "undefined instruction" exceptions. That is, this test succeeds when this
// test finishes without a crash.
TEST(CPUPA, RunExtendedInstructions) {
  // Retrieve the CPU information.
  internal::base::CPU cpu;
#if PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY)

  ASSERT_TRUE(cpu.has_mmx());
  ASSERT_TRUE(cpu.has_sse());
  ASSERT_TRUE(cpu.has_sse2());
  ASSERT_TRUE(cpu.has_sse3());

// GCC and clang instruction test.
#if PA_BUILDFLAG(PA_COMPILER_GCC)
  // Execute an MMX instruction.
  __asm__ __volatile__("emms\n" : : : "mm0");

  // Execute an SSE instruction.
  __asm__ __volatile__("xorps %%xmm0, %%xmm0\n" : : : "xmm0");

  // Execute an SSE 2 instruction.
  __asm__ __volatile__("psrldq $0, %%xmm0\n" : : : "xmm0");

  // Execute an SSE 3 instruction.
  __asm__ __volatile__("addsubpd %%xmm0, %%xmm0\n" : : : "xmm0");

  if (cpu.has_ssse3()) {
    // Execute a Supplimental SSE 3 instruction.
    __asm__ __volatile__("psignb %%xmm0, %%xmm0\n" : : : "xmm0");
  }

  if (cpu.has_sse41()) {
    // Execute an SSE 4.1 instruction.
    __asm__ __volatile__("pmuldq %%xmm0, %%xmm0\n" : : : "xmm0");
  }

  if (cpu.has_sse42()) {
    // Execute an SSE 4.2 instruction.
    __asm__ __volatile__("crc32 %%eax, %%eax\n" : : : "eax");
  }

  if (cpu.has_popcnt()) {
    // Execute a POPCNT instruction.
    __asm__ __volatile__("popcnt %%eax, %%eax\n" : : : "eax");
  }

  if (cpu.has_avx()) {
    // Execute an AVX instruction.
    __asm__ __volatile__("vzeroupper\n" : : : "xmm0");
  }

  if (cpu.has_fma3()) {
    // Execute a FMA3 instruction.
    __asm__ __volatile__("vfmadd132ps %%xmm0, %%xmm0, %%xmm0\n" : : : "xmm0");
  }

  if (cpu.has_avx2()) {
    // Execute an AVX 2 instruction.
    __asm__ __volatile__("vpunpcklbw %%ymm0, %%ymm0, %%ymm0\n" : : : "xmm0");
  }

  if (cpu.has_pku()) {
    // rdpkru
    uint32_t pkru;
    __asm__ __volatile__(".byte 0x0f,0x01,0xee\n"
                         : "=a"(pkru)
                         : "c"(0), "d"(0));
  }
// Visual C 32 bit and ClangCL 32/64 bit test.
#elif PA_BUILDFLAG(PA_COMPILER_MSVC) &&   \
    (PA_BUILDFLAG(PA_ARCH_CPU_32_BITS) || \
     (PA_BUILDFLAG(PA_ARCH_CPU_64_BITS) && defined(__clang__)))

  // Execute an MMX instruction.
  __asm emms;

  // Execute an SSE instruction.
  __asm xorps xmm0, xmm0;

  // Execute an SSE 2 instruction.
  __asm psrldq xmm0, 0;

  // Execute an SSE 3 instruction.
  __asm addsubpd xmm0, xmm0;

  if (cpu.has_ssse3()) {
    // Execute a Supplimental SSE 3 instruction.
    __asm psignb xmm0, xmm0;
  }

  if (cpu.has_sse41()) {
    // Execute an SSE 4.1 instruction.
    __asm pmuldq xmm0, xmm0;
  }

  if (cpu.has_sse42()) {
    // Execute an SSE 4.2 instruction.
    __asm crc32 eax, eax;
  }

  if (cpu.has_popcnt()) {
    // Execute a POPCNT instruction.
    __asm popcnt eax, eax;
  }

  if (cpu.has_avx()) {
    // Execute an AVX instruction.
    __asm vzeroupper;
  }

  if (cpu.has_fma3()) {
    // Execute an AVX instruction.
    __asm vfmadd132ps xmm0, xmm0, xmm0;
  }

  if (cpu.has_avx2()) {
    // Execute an AVX 2 instruction.
    __asm vpunpcklbw ymm0, ymm0, ymm0
  }
#endif  // PA_BUILDFLAG(PA_COMPILER_GCC)
#endif  // PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY)

#if PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
  // Check that the CPU is correctly reporting support for the Armv8.5-A memory
  // tagging extension. The new MTE instructions aren't encoded in NOP space
  // like BTI/Pointer Authentication and will crash older cores with a SIGILL if
  // used incorrectly. This test demonstrates how it should be done and that
  // this approach works.
  if (cpu.has_mte()) {
#if !defined(__ARM_FEATURE_MEMORY_TAGGING)
    // In this section, we're running on an MTE-compatible core, but we're
    // building this file without MTE support. Fail this test to indicate that
    // there's a problem with the base/ build configuration.
    GTEST_FAIL()
        << "MTE support detected (but base/ built without MTE support)";
#else
    char ptr[32];
    uint64_t val;
    // Execute a trivial MTE instruction. Normally, MTE should be used via the
    // intrinsics documented at
    // https://developer.arm.com/documentation/101028/0012/10--Memory-tagging-intrinsics,
    // this test uses the irg (Insert Random Tag) instruction directly to make
    // sure that it's not optimized out by the compiler.
    __asm__ __volatile__("irg %0, %1" : "=r"(val) : "r"(ptr));
#endif  // __ARM_FEATURE_MEMORY_TAGGING
  }
#endif  // ARCH_CPU_ARM64
}

}  // namespace partition_alloc
