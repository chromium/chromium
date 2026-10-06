// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/aligned_memory.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/bucket_lookup.h"              // nogncheck
#include "partition_alloc/partition_alloc_constants.h"  // nogncheck
#endif

namespace base {

namespace {

bool IsFilledWith(span<const uint8_t> bytes, uint8_t value) {
  return std::ranges::all_of(bytes,
                             [value](uint8_t byte) { return byte == value; });
}

// Checks `AlignedCalloc()` of `kSize` bytes. The memory is accessed as an
// `std::array` of the allocated size, so that its type bounds the accesses.
template <size_t kSize>
void CheckAlignedCalloc(size_t alignment) {
  SCOPED_TRACE(testing::Message()
               << "size: " << kSize << ", alignment: " << alignment);
  using Bytes = std::array<uint8_t, kSize>;

  // Frees memory with non-zero bytes, which `AlignedCalloc()` can reuse.
  // `Alias()` keeps the compiler from removing the writes.
  auto* dirty = static_cast<Bytes*>(AlignedAlloc(sizeof(Bytes), alignment));
  std::ranges::fill(*dirty, uint8_t{0xA5});
  debug::Alias(dirty);
  AlignedFree(dirty);

  auto* zeroed =
      static_cast<Bytes*>(AlignedCalloc(1, sizeof(Bytes), alignment));
  EXPECT_TRUE(IsAligned(zeroed, alignment));
  EXPECT_TRUE(IsFilledWith(*zeroed, 0));
  AlignedFree(zeroed);
}

}  // namespace

TEST(AlignedMemoryTest, AlignedUninit) {
  {
    base::AlignedHeapArray<char> h = AlignedUninit<char>(8, 32);
    EXPECT_EQ(h.size(), 8u);
    EXPECT_TRUE(IsAligned(h.data(), 32));
  }
  {
    base::AlignedHeapArray<int16_t> h = AlignedUninit<int16_t>(8, 32);
    EXPECT_EQ(h.size(), 8u);
    EXPECT_TRUE(IsAligned(h.data(), 32));
  }
}

TEST(AlignedMemoryTest, AlignedUninitCharArray) {
  auto [h, s] = AlignedUninitCharArray<int16_t>(8, 32);
  static_assert(std::same_as<base::AlignedHeapArray<char>, decltype(h)>);
  static_assert(std::same_as<base::span<int16_t>, decltype(s)>);
  EXPECT_EQ(h.size(), 8u * sizeof(int16_t));
  EXPECT_TRUE(IsAligned(h.data(), 32));
  EXPECT_EQ(s.size(), 8u);
  EXPECT_TRUE(IsAligned(s.data(), 32));
}

template <class T>
concept CanBeAlignedZeroed = requires { AlignedZeroed<T>(1); };
static_assert(CanBeAlignedZeroed<uint8_t>);
static_assert(CanBeAlignedZeroed<float>);
static_assert(CanBeAlignedZeroed<std::byte>);
// A null pointer, for example, doesn't have to be all zero bytes.
static_assert(!CanBeAlignedZeroed<int*>);

TEST(AlignedMemoryTest, AlignedZeroed) {
  {
    base::AlignedHeapArray<std::byte> h = AlignedZeroed<std::byte>(8, 32);
    EXPECT_EQ(h.size(), 8u);
    EXPECT_TRUE(IsAligned(h.data(), 32));
    EXPECT_TRUE(std::ranges::all_of(
        h, [](std::byte value) { return value == std::byte{0}; }));
  }
  {
    // The default alignment is at least the alignment of a pointer.
    base::AlignedHeapArray<float> h = AlignedZeroed<float>(1000);
    EXPECT_EQ(h.size(), 1000u);
    EXPECT_TRUE(IsAligned(h.data(), alignof(void*)));
    EXPECT_TRUE(
        std::ranges::all_of(h, [](float value) { return value == 0.0f; }));
  }
  {
    // Large enough for `AlignedCalloc()` to use the allocator shim's calloc
    // function when PartitionAlloc is malloc.
    base::AlignedHeapArray<uint8_t> h = AlignedZeroed<uint8_t>(4 << 20, 4096);
    EXPECT_EQ(h.size(), size_t{4} << 20);
    EXPECT_TRUE(IsAligned(h.data(), 4096));
    EXPECT_TRUE(IsFilledWith(h, 0));
  }
}

TEST(AlignedMemoryTest, DynamicAllocation) {
  void* p = AlignedAlloc(8, 8);
  ASSERT_TRUE(p);
  EXPECT_TRUE(IsAligned(p, 8));
  UNSAFE_TODO(memset(p, 0, 8));  // Fill to check allocated size under ASAN.
  AlignedFree(p);

  p = AlignedAlloc(8, 16);
  ASSERT_TRUE(p);
  EXPECT_TRUE(IsAligned(p, 16));
  UNSAFE_TODO(memset(p, 0, 8));  // Fill to check allocated size under ASAN.
  AlignedFree(p);

  p = AlignedAlloc(8, 256);
  ASSERT_TRUE(p);
  EXPECT_TRUE(IsAligned(p, 256));
  UNSAFE_TODO(memset(p, 0, 8));  // Fill to check allocated size under ASAN.
  AlignedFree(p);

  p = AlignedAlloc(8, 4096);
  ASSERT_TRUE(p);
  EXPECT_TRUE(IsAligned(p, 4096));
  UNSAFE_TODO(memset(p, 0, 8));  // Fill to check allocated size under ASAN.
  AlignedFree(p);
}

TEST(AlignedMemoryTest, ScopedDynamicAllocation) {
  std::unique_ptr<float, AlignedFreeDeleter> p(
      static_cast<float*>(AlignedAlloc(8, 8)));
  EXPECT_TRUE(p.get());
  EXPECT_TRUE(IsAligned(p.get(), 8));

  // Make sure IsAligned() can check const pointers as well.
  const float* const_p = p.get();
  EXPECT_TRUE(IsAligned(const_p, 8));
}

TEST(AlignedMemoryTest, AlignedCalloc) {
  for (size_t alignment : {size_t{16}, size_t{64}, size_t{4096}}) {
    CheckAlignedCalloc<1>(alignment);
    CheckAlignedCalloc<100>(alignment);
    CheckAlignedCalloc<4097>(alignment);
    CheckAlignedCalloc<(4 << 20)>(alignment);
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    // `AlignedCalloc()` uses the allocator shim's calloc function for the sizes
    // that PartitionAlloc always direct-maps, and `AlignedAlloc()` and
    // `memset()` for the other sizes. Test both sides of this threshold.
    constexpr size_t kMaxBucketSize =
        partition_alloc::BucketIndexLookup::kMaxBucketSize;
    static_assert(!partition_alloc::IsAlwaysDirectMapped(kMaxBucketSize));
    static_assert(partition_alloc::IsAlwaysDirectMapped(kMaxBucketSize + 1));
    CheckAlignedCalloc<kMaxBucketSize>(alignment);
    CheckAlignedCalloc<kMaxBucketSize + 1>(alignment);
#endif
  }
}

TEST(AlignedMemoryTest, AlignedCallocMultipleItems) {
  using Item = std::array<uint8_t, 1000>;
  auto* items =
      static_cast<std::array<Item, 3>*>(AlignedCalloc(3, sizeof(Item), 64));
  EXPECT_TRUE(IsAligned(items, 64));
  for (const Item& item : *items) {
    EXPECT_TRUE(IsFilledWith(item, 0));
  }
  AlignedFree(items);
}

TEST(AlignedMemoryDeathTest, AlignedCallocOverflow) {
  BASE_EXPECT_DEATH(AlignedCalloc(std::numeric_limits<size_t>::max(), 2, 64),
                    "");
}

TEST(AlignedMemoryTest, IsAligned) {
  // Check alignment around powers of two.
  for (int i = 0; i < 64; ++i) {
    const uint64_t n = static_cast<uint64_t>(1) << i;

    // Walk back down all lower powers of two checking alignment.
    for (int j = i - 1; j >= 0; --j) {
      // n is aligned on all powers of two less than or equal to 2^i.
      EXPECT_TRUE(IsAligned(n, n >> j))
          << "Expected " << n << " to be " << (n >> j) << " aligned";

      // Also, n - 1 should not be aligned on ANY lower power of two except 1
      // (but since we're starting from i - 1 we don't test that case here.
      EXPECT_FALSE(IsAligned(n - 1, n >> j))
          << "Expected " << (n - 1) << " to NOT be " << (n >> j) << " aligned";
    }
  }

  // And a few hard coded smoke tests for completeness:
  EXPECT_TRUE(IsAligned(4, 2));
  EXPECT_TRUE(IsAligned(8, 4));
  EXPECT_TRUE(IsAligned(8, 2));
  EXPECT_TRUE(IsAligned(0x1000, 4 << 10));
  EXPECT_TRUE(IsAligned(0x2000, 8 << 10));
  EXPECT_TRUE(IsAligned(1, 1));
  EXPECT_TRUE(IsAligned(7, 1));
  EXPECT_TRUE(IsAligned(reinterpret_cast<void*>(0x1000), 4 << 10));
  EXPECT_TRUE(IsAligned(reinterpret_cast<int*>(0x1000), 4 << 10));

  EXPECT_FALSE(IsAligned(3, 2));
  EXPECT_FALSE(IsAligned(7, 4));
  EXPECT_FALSE(IsAligned(7, 2));
  EXPECT_FALSE(IsAligned(0x1001, 4 << 10));
  EXPECT_FALSE(IsAligned(0x999, 8 << 10));
  EXPECT_FALSE(IsAligned(7, 8));
}

}  // namespace base
