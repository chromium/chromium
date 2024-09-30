// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/cpu.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/protected_memory_buildflags.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
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

  if (cpu.has_avx_vnni()) {
    // Execute an AVX VNNI instruction. {vex} prevents EVEX encoding, which
    // would shift it to AVX512 VNNI.
    __asm__ __volatile__("%{vex%} vpdpbusd %%ymm0, %%ymm0, %%ymm0\n"
                         :
                         :
                         : "ymm0");
  }

  if (cpu.has_avx512_f()) {
    // Execute an AVX-512 Foundation (F) instruction.
    __asm__ __volatile__("vpxorq %%zmm0, %%zmm0, %%zmm0\n" : : : "zmm0");
  }

  if (cpu.has_avx512_bw()) {
    // Execute an AVX-512 Byte & Word (BW) instruction.
    __asm__ __volatile__("vpabsw %%zmm0, %%zmm0\n" : : : "zmm0");
  }

  if (cpu.has_avx512_vnni()) {
    // Execute an AVX-512 VNNI instruction.
    __asm__ __volatile__("vpdpbusd %%zmm0, %%zmm0, %%zmm0\n" : : : "zmm0");
  }

  if (cpu.has_pku()) {
    // rdpkru
    uint32_t pkru;
    __asm__ __volatile__(".byte 0x0f,0x01,0xee\n"
                         : "=a"(pkru)
                         : "c"(0), "d"(0));
  }
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

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
TEST(CPUDeathTest, VerifyModifyingCPUInstanceNoAllocationCrashes) {
  const base::CPU& cpu = base::CPU::GetInstanceNoAllocation();
  uint8_t* const bytes =
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&cpu));

  // We try and flip a couple of bits and expect the test to die immediately.
  // Checks are limited to every 15th byte, otherwise the tests run into
  // time-outs.
  for (size_t byte_index = 0; byte_index < sizeof(cpu); byte_index += 15) {
    const size_t local_bit_index = byte_index % 8;
    EXPECT_CHECK_DEATH_WITH(bytes[byte_index] ^= (0x01 << local_bit_index), "");
  }
}
#endif
