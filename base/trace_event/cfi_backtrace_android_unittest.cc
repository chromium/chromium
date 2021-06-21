// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/cfi_backtrace_android.h"

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

namespace {

void* GetPC() {
  return __builtin_return_address(0);
}

}  // namespace

TEST(CFIBacktraceAndroidTest, TestUnwinding) {
  auto* unwinder = CFIBacktraceAndroid::GetInitializedInstance();
  EXPECT_TRUE(unwinder->can_unwind_stack_frames());
  EXPECT_GT(unwinder->executable_start_addr(), 0u);
  EXPECT_GT(unwinder->executable_end_addr(), unwinder->executable_start_addr());
  EXPECT_GT(unwinder->cfi_mmap_->length(), 0u);

  const size_t kMaxFrames = 100;
  const void* frames[kMaxFrames];
  size_t unwind_count = unwinder->Unwind(frames, kMaxFrames);
  // Expect at least 2 frames in the result.
  ASSERT_GT(unwind_count, 2u);
  EXPECT_LE(unwind_count, kMaxFrames);

  const size_t kMaxCurrentFuncCodeSize = 50;
  const uintptr_t current_pc = reinterpret_cast<uintptr_t>(GetPC());
  const uintptr_t actual_frame = reinterpret_cast<uintptr_t>(frames[2]);
  EXPECT_NEAR(current_pc, actual_frame, kMaxCurrentFuncCodeSize);

  for (size_t i = 0; i < unwind_count; ++i) {
    EXPECT_GT(reinterpret_cast<uintptr_t>(frames[i]),
              unwinder->executable_start_addr());
    EXPECT_LT(reinterpret_cast<uintptr_t>(frames[i]),
              unwinder->executable_end_addr());
  }
}

// Flaky: https://bugs.chromium.org/p/chromium/issues/detail?id=829555
TEST(CFIBacktraceAndroidTest, DISABLED_TestFindCFIRow) {
  auto* unwinder = CFIBacktraceAndroid::GetInitializedInstance();
  /* Input is generated from the CFI file:
  STACK CFI INIT 1000 500
  STACK CFI 1002 .cfa: sp 272 + .ra: .cfa -4 + ^ r4: .cfa -16 +
  STACK CFI 1008 .cfa: sp 544 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  STACK CFI 1040 .cfa: sp 816 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  STACK CFI 1050 .cfa: sp 816 + .ra: .cfa -8 + ^ r4: .cfa -16 + ^
  STACK CFI 1080 .cfa: sp 544 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^

  STACK CFI INIT 2000 22
  STACK CFI 2004 .cfa: sp 16 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  STACK CFI 2008 .cfa: sp 16 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^

  STACK CFI INIT 2024 100
  STACK CFI 2030 .cfa: sp 48 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  STACK CFI 2100 .cfa: sp 64 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^

  STACK CFI INIT 2200 10
  STACK CFI 2204 .cfa: sp 44 + .ra: .cfa -8 + ^ r4: .cfa -16 + ^
  */
  uint16_t input[] = {// UNW_INDEX size
                      0x07, 0x0,

                      // UNW_INDEX address column (4 byte rows).
                      0x1000, 0x0, 0x1502, 0x0, 0x2000, 0x0, 0x2024, 0x0,
                      0x2126, 0x0, 0x2200, 0x0, 0x2212, 0x0,

                      // UNW_INDEX index column (2 byte rows).
                      0x0, 0xffff, 0xb, 0x10, 0xffff, 0x15, 0xffff,

                      // UNW_DATA table.
                      0x5, 0x2, 0x111, 0x8, 0x220, 0x40, 0x330, 0x50, 0x332,
                      0x80, 0x220, 0x2, 0x4, 0x13, 0x8, 0x13, 0x2, 0xc, 0x33,
                      0xdc, 0x40, 0x1, 0x4, 0x2e};
  FilePath temp_path;
  CreateTemporaryFile(&temp_path);
  EXPECT_EQ(
      static_cast<int>(sizeof(input)),
      WriteFile(temp_path, reinterpret_cast<char*>(input), sizeof(input)));

  unwinder->cfi_mmap_.reset(new MemoryMappedFile());
  ASSERT_TRUE(unwinder->cfi_mmap_->Initialize(temp_path));
  unwinder->ParseCFITables();

  CFIBacktraceAndroid::CFIRow cfi_row = {0};
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x01, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x100, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x1502, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x3000, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x2024, &cfi_row));
  EXPECT_FALSE(unwinder->FindCFIRowForPC(0x2212, &cfi_row));

  const CFIBacktraceAndroid::CFIRow kRow1 = {0x110, 0x4};
  const CFIBacktraceAndroid::CFIRow kRow2 = {0x220, 0x4};
  const CFIBacktraceAndroid::CFIRow kRow3 = {0x220, 0x8};
  const CFIBacktraceAndroid::CFIRow kRow4 = {0x30, 0xc};
  const CFIBacktraceAndroid::CFIRow kRow5 = {0x2c, 0x8};
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1002, &cfi_row));
  EXPECT_EQ(kRow1, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1003, &cfi_row));
  EXPECT_EQ(kRow1, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1008, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1009, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1039, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1080, &cfi_row));
  EXPECT_EQ(kRow3, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1100, &cfi_row));
  EXPECT_EQ(kRow3, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2050, &cfi_row));
  EXPECT_EQ(kRow4, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2208, &cfi_row));
  EXPECT_EQ(kRow5, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2210, &cfi_row));
  EXPECT_EQ(kRow5, cfi_row);

  // Test if cache is used on the future calls to Find, all addresses should
  // have different hash. Resetting the memory map to make sure it is never
  // accessed in Find().
  unwinder->cfi_mmap_.reset(new MemoryMappedFile());
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1002, &cfi_row));
  EXPECT_EQ(kRow1, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1003, &cfi_row));
  EXPECT_EQ(kRow1, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1008, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1009, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1039, &cfi_row));
  EXPECT_EQ(kRow2, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1080, &cfi_row));
  EXPECT_EQ(kRow3, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x1100, &cfi_row));
  EXPECT_EQ(kRow3, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2050, &cfi_row));
  EXPECT_EQ(kRow4, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2208, &cfi_row));
  EXPECT_EQ(kRow5, cfi_row);
  EXPECT_TRUE(unwinder->FindCFIRowForPC(0x2210, &cfi_row));
  EXPECT_EQ(kRow5, cfi_row);
}

TEST(CFIBacktraceAndroidTest, TestCFICache) {
  // Use ASSERT macros in this function since they are in loop and using EXPECT
  // prints too many failures.
  CFIBacktraceAndroid::CFICache cache;
  CFIBacktraceAndroid::CFIRow cfi;

  // Empty cache should not find anything.
  EXPECT_FALSE(cache.Find(1, &cfi));

  // Insert 1 - 2*kLimit
  for (uintptr_t i = 1; i <= 2 * cache.kLimit; ++i) {
    CFIBacktraceAndroid::CFIRow val = {static_cast<uint16_t>(4 * i),
                                       static_cast<uint16_t>(2 * i)};
    cache.Add(i, val);
    ASSERT_TRUE(cache.Find(i, &cfi));
    ASSERT_EQ(cfi, val);

    // Inserting more than kLimit items evicts |i - cache.kLimit| from cache.
    if (i >= cache.kLimit)
      ASSERT_FALSE(cache.Find(i - cache.kLimit, &cfi));
  }
  // Cache contains kLimit+1 - 2*kLimit.

  // Check that 1 - kLimit cannot be found.
  for (uintptr_t i = 1; i <= cache.kLimit; ++i) {
    ASSERT_FALSE(cache.Find(i, &cfi));
  }

  // Check if kLimit+1 - 2*kLimit still exists in cache.
  for (uintptr_t i = cache.kLimit + 1; i <= 2 * cache.kLimit; ++i) {
    CFIBacktraceAndroid::CFIRow val = {static_cast<uint16_t>(4 * i),
                                       static_cast<uint16_t>(2 * i)};
    ASSERT_TRUE(cache.Find(i, &cfi));
    ASSERT_EQ(cfi, val);
  }

  // Insert 2*kLimit+1, will evict kLimit.
  cfi = {1, 1};
  cache.Add(2 * cache.kLimit + 1, cfi);
  EXPECT_TRUE(cache.Find(2 * cache.kLimit + 1, &cfi));
  EXPECT_FALSE(cache.Find(cache.kLimit + 1, &cfi));
  // Cache contains kLimit+1 - 2*kLimit.
}

}  // namespace trace_event
}  // namespace base
