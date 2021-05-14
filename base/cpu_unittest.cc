// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests whether we can run extended instructions represented by the CPU
// information. This test actually executes some extended instructions (such as
// MMX, SSE, etc.) supported by the CPU and sees we can run them without
// "undefined instruction" exceptions. That is, this test succeeds when this
// test finishes without a crash.
TEST(CPU, RunExtendedInstructions) {
  // Retrieve the CPU information.
  base::CPU cpu;
#if defined(ARCH_CPU_X86_FAMILY)

  ASSERT_TRUE(cpu.has_mmx());
  ASSERT_TRUE(cpu.has_sse());
  ASSERT_TRUE(cpu.has_sse2());
  ASSERT_TRUE(cpu.has_sse3());

// GCC and clang instruction test.
#if defined(COMPILER_GCC)
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

  if (cpu.has_avx2()) {
    // Execute an AVX 2 instruction.
    __asm__ __volatile__("vpunpcklbw %%ymm0, %%ymm0, %%ymm0\n" : : : "xmm0");
  }
// Visual C 32 bit and ClangCL 32/64 bit test.
#elif defined(COMPILER_MSVC) && (defined(ARCH_CPU_32_BITS) || \
      (defined(ARCH_CPU_64_BITS) && defined(__clang__)))

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

  if (cpu.has_avx2()) {
    // Execute an AVX 2 instruction.
    __asm vpunpcklbw ymm0, ymm0, ymm0
  }
#endif  // defined(COMPILER_GCC)
#endif  // defined(ARCH_CPU_X86_FAMILY)

#if defined(ARCH_CPU_ARM64)
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

// For https://crbug.com/249713
TEST(CPU, BrandAndVendorContainsNoNUL) {
  base::CPU cpu;
  EXPECT_FALSE(base::Contains(cpu.cpu_brand(), '\0'));
  EXPECT_FALSE(base::Contains(cpu.vendor_name(), '\0'));
}

#if defined(ARCH_CPU_X86_FAMILY)
// Tests that we compute the correct CPU family and model based on the vendor
// and CPUID signature.
TEST(CPU, X86FamilyAndModel) {
  base::internal::X86ModelInfo info;

  // Check with an Intel Skylake signature.
  info = base::internal::ComputeX86FamilyAndModel("GenuineIntel", 0x000406e3);
  EXPECT_EQ(info.family, 6);
  EXPECT_EQ(info.model, 78);
  EXPECT_EQ(info.ext_family, 0);
  EXPECT_EQ(info.ext_model, 4);

  // Check with an Intel Airmont signature.
  info = base::internal::ComputeX86FamilyAndModel("GenuineIntel", 0x000406c2);
  EXPECT_EQ(info.family, 6);
  EXPECT_EQ(info.model, 76);
  EXPECT_EQ(info.ext_family, 0);
  EXPECT_EQ(info.ext_model, 4);

  // Check with an Intel Prescott signature.
  info = base::internal::ComputeX86FamilyAndModel("GenuineIntel", 0x00000f31);
  EXPECT_EQ(info.family, 15);
  EXPECT_EQ(info.model, 3);
  EXPECT_EQ(info.ext_family, 0);
  EXPECT_EQ(info.ext_model, 0);

  // Check with an AMD Excavator signature.
  info = base::internal::ComputeX86FamilyAndModel("AuthenticAMD", 0x00670f00);
  EXPECT_EQ(info.family, 21);
  EXPECT_EQ(info.model, 112);
  EXPECT_EQ(info.ext_family, 6);
  EXPECT_EQ(info.ext_model, 7);
}
#endif  // defined(ARCH_CPU_X86_FAMILY)

#if defined(ARCH_CPU_ARM_FAMILY) && \
    (defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_CHROMEOS))
TEST(CPU, ARMImplementerAndPartNumber) {
  base::CPU cpu;

  const std::string& cpu_brand = cpu.cpu_brand();

  // Some devices, including on the CQ, do not report a cpu_brand
  // https://crbug.com/1166533 and https://crbug.com/1167123.
  EXPECT_EQ(cpu_brand, base::TrimWhitespaceASCII(cpu_brand, base::TRIM_ALL));
  EXPECT_GT(cpu.implementer(), 0u);
  EXPECT_GT(cpu.part_number(), 0u);
}
#endif  // defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_LINUX) ||
        // defined(OS_ANDROID) || defined(OS_CHROMEOS))
