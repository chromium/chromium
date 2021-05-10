// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/scan_loop.h"

#include "base/cpu.h"
#include "build/build_config.h"

#include "testing/gtest/include/gtest/gtest.h"

#if defined(PA_HAS_64_BITS_POINTERS)

namespace base {
namespace internal {

namespace {

enum class Cage { kOn, kOff };

class TestScanLoop final : public ScanLoop<TestScanLoop> {
  friend class ScanLoop<TestScanLoop>;

 public:
  TestScanLoop(SimdSupport ss, Cage cage)
      : ScanLoop(ss), with_cage_(cage == Cage::kOn) {}

  size_t visited() const { return visited_; }

 private:
  static constexpr uintptr_t kCageMask = 0xffffff0000000000;
  static constexpr uintptr_t kBasePtr = 0x1234560000000000;

  bool WithCage() const { return with_cage_; }
  uintptr_t CageBase() const { return kBasePtr; }
  static constexpr uintptr_t CageMask() { return kCageMask; }

  void CheckPointer(uintptr_t maybe_ptr) { ++visited_; }
  void CheckPointerNoGigaCage(uintptr_t maybe_ptr) { ++visited_; }

  size_t visited_ = 0;
  bool with_cage_ = false;
};

static constexpr uintptr_t kValidPtr = 0x123456789abcdef0;
static constexpr uintptr_t kInvalidPtr = 0xaaaaaaaaaaaaaaaa;
static constexpr uintptr_t kZeroPtr = 0x0;

template <size_t Alignment, typename... Args>
void RunOnRangeWithAlignment(TestScanLoop& sl, Args... args) {
  alignas(Alignment) uintptr_t range[] = {args...};
  sl.Run(std::begin(range), std::end(range));
}

}  // namespace

TEST(ScanLoopTest, UnvectorizedWithCage) {
  {
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOn);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(0u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOn);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kValidPtr, kInvalidPtr);
    EXPECT_EQ(1u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOn);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kValidPtr, kValidPtr);
    EXPECT_EQ(2u, sl.visited());
  }
  {
    // Make sure zeros are skipped.
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOn);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kValidPtr, kZeroPtr);
    EXPECT_EQ(1u, sl.visited());
  }
}

TEST(ScanLoopTest, UnvectorizedNoCage) {
  // Without the cage all non-zero pointers are visited.
  {
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOff);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(3u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOff);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kValidPtr, kInvalidPtr);
    EXPECT_EQ(3u, sl.visited());
  }
  {
    // Make sure zeros are skipped.
    TestScanLoop sl(SimdSupport::kUnvectorized, Cage::kOff);
    RunOnRangeWithAlignment<8>(sl, kInvalidPtr, kZeroPtr, kValidPtr);
    EXPECT_EQ(2u, sl.visited());
  }
}

#if defined(ARCH_CPU_X86_64)
TEST(ScanLoopTest, VectorizedSSE4) {
  base::CPU cpu;
  if (!cpu.has_sse41())
    return;
  {
    TestScanLoop sl(SimdSupport::kSSE41, Cage::kOn);
    RunOnRangeWithAlignment<16>(sl, kInvalidPtr, kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(0u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kSSE41, Cage::kOn);
    RunOnRangeWithAlignment<16>(sl, kValidPtr, kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(1u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kSSE41, Cage::kOn);
    RunOnRangeWithAlignment<16>(sl, kValidPtr, kValidPtr, kInvalidPtr);
    EXPECT_EQ(2u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kSSE41, Cage::kOn);
    RunOnRangeWithAlignment<16>(sl, kValidPtr, kValidPtr, kValidPtr);
    EXPECT_EQ(3u, sl.visited());
  }
}

TEST(ScanLoopTest, VectorizedAVX2) {
  base::CPU cpu;
  if (!cpu.has_avx2())
    return;
  {
    TestScanLoop sl(SimdSupport::kAVX2, Cage::kOn);
    RunOnRangeWithAlignment<32>(sl, kInvalidPtr, kInvalidPtr, kInvalidPtr,
                                kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(0u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2, Cage::kOn);
    RunOnRangeWithAlignment<32>(sl, kValidPtr, kInvalidPtr, kInvalidPtr,
                                kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(1u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2, Cage::kOn);
    RunOnRangeWithAlignment<32>(sl, kValidPtr, kValidPtr, kInvalidPtr,
                                kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(2u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2, Cage::kOn);
    RunOnRangeWithAlignment<32>(sl, kValidPtr, kValidPtr, kValidPtr,
                                kInvalidPtr, kInvalidPtr);
    EXPECT_EQ(3u, sl.visited());
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2, Cage::kOn);
    RunOnRangeWithAlignment<32>(sl, kValidPtr, kValidPtr, kValidPtr, kValidPtr,
                                kInvalidPtr);
    EXPECT_EQ(4u, sl.visited());
  }
  {
    // Check that the residual pointer is also visited.
    TestScanLoop sl(SimdSupport::kAVX2, Cage::kOn);
    RunOnRangeWithAlignment<32>(sl, kValidPtr, kValidPtr, kValidPtr, kValidPtr,
                                kValidPtr);
    EXPECT_EQ(5u, sl.visited());
  }
}
#endif  // defined(ARCH_CPU_X86_64)

}  // namespace internal
}  // namespace base

#endif  // defined(PA_HAS_64_BITS_POINTERS)
