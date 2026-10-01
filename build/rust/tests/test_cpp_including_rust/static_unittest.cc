// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "build/rust/tests/test_rust_static_library/src/lib.rs.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/partition_address_space.h"  // nogncheck
#else
#include "partition_alloc/address_pool_manager_bitmap.h"  // nogncheck
#endif
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/bucket_lookup.h"              // nogncheck
#include "partition_alloc/partition_alloc_constants.h"  // nogncheck
#endif

TEST(RustStaticTest, CppCallingIntoRust_BasicFFI) {
  EXPECT_EQ(7, add_two_ints_via_rust(3, 4));
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)

TEST(RustStaticTest, RustComponentUsesPartitionAlloc) {
  // Verify that PartitionAlloc is consistently used in C++ and Rust.
  auto cpp_allocated_int = std::make_unique<int>();
  SomeStruct* rust_allocated_ptr = allocate_via_rust().into_raw();
  EXPECT_EQ(partition_alloc::IsManagedByPartitionAlloc(
                reinterpret_cast<uintptr_t>(rust_allocated_ptr)),
            partition_alloc::IsManagedByPartitionAlloc(
                reinterpret_cast<uintptr_t>(cpp_allocated_int.get())));
  rust::Box<SomeStruct>::from_raw(rust_allocated_ptr);
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

TEST(RustStaticTest, AllocAligned) {
  alloc_aligned();
}

TEST(RustStaticTest, RustLargeAllocationFailure) {
  // A small allocation that should always succeed. If allocation succeeds, we
  // get true back.
  EXPECT_TRUE(allocate_huge_via_rust(100u, 1u));

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // We only do these tests when using PA, as the system allocator will not fail
  // on large allocations (unless it is really OOM).

  // PartitionAlloc currently limits all allocations to no more than i32::MAX
  // elements, so the allocation will fail. If done through normal malloc(),
  // PA will crash when an allocation fails rather than return null, but Rust
  // can be trusted to handle failure without introducing null derefs so this
  // should fail gracefully.
  size_t max_size = partition_alloc::MaxAllocationSize();
  EXPECT_FALSE(allocate_huge_via_rust(max_size + 1u, 4u));

  // Same as above but with an alignment larger than PartitionAlloc's default
  // alignment, which goes down a different path.
  size_t big_alignment = alignof(std::max_align_t) * 2u;
  EXPECT_FALSE(allocate_huge_via_rust(max_size + 1u, big_alignment));

  // PartitionAlloc will crash if given an alignment larger than this. The
  // allocation hooks handle it gracefully.
  size_t max_alignment = partition_alloc::internal::kMaxSupportedAlignment;
  EXPECT_FALSE(allocate_huge_via_rust(100u, max_alignment * 2u));

  // Repeat the test but with alloc_zeroed().
  EXPECT_TRUE(allocate_zeroed_huge_via_rust(100u, 1u));
  EXPECT_FALSE(allocate_zeroed_huge_via_rust(max_size + 1u, 4u));
  EXPECT_FALSE(allocate_zeroed_huge_via_rust(max_size + 1u, big_alignment));
  EXPECT_FALSE(allocate_zeroed_huge_via_rust(100u, max_alignment * 2u));

  // Repeat the test but with realloc().
  EXPECT_TRUE(reallocate_huge_via_rust(100u, 1u));
  EXPECT_FALSE(reallocate_huge_via_rust(max_size + 1u, 4u));
  EXPECT_FALSE(reallocate_huge_via_rust(max_size + 1u, big_alignment));
  // Note: We don't test with `max_alignment * 2` since the initial allocation
  // will always fail, the realloc can't happen anyway.

#endif
}

TEST(RustStaticTest, RustAllocZeroed) {
  std::vector<size_t> sizes = {1u, 100u, 4097u, 4u << 20};
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // alloc_zeroed() uses calloc() for the sizes that PartitionAlloc always
  // direct-maps, and alloc() and memset() for the other sizes. Also test the
  // sizes on both sides of this threshold.
  constexpr size_t kMaxBucketSize =
      partition_alloc::BucketIndexLookup::kMaxBucketSize;
  static_assert(!partition_alloc::IsAlwaysDirectMapped(kMaxBucketSize));
  static_assert(partition_alloc::IsAlwaysDirectMapped(kMaxBucketSize + 1));
  sizes.push_back(kMaxBucketSize);
  sizes.push_back(kMaxBucketSize + 1);
#endif

  // The larger alignment uses the aligned allocation functions.
  size_t big_alignment = alignof(std::max_align_t) * 2u;
  for (size_t size : sizes) {
    for (size_t align : {size_t{1}, big_alignment}) {
      // Before alloc_zeroed(), this frees memory with non-zero bytes, which the
      // allocator can reuse. So this checks that alloc_zeroed() zeroes it,
      // though a pass doesn't prove that it always does.
      EXPECT_TRUE(allocate_zeroed_via_rust_returns_zeros(size, align))
          << "size: " << size << ", align: " << align;
    }
  }
}
