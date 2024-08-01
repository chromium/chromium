// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "build/rust/tests/test_rust_shared_library/src/lib.rs.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/partition_address_space.h"
#else
#include "partition_alloc/address_pool_manager_bitmap.h"
#endif

TEST(RustSharedTest, CppCallingIntoRust_BasicFFI) {
  EXPECT_EQ(7, add_two_ints_via_rust(3, 4));
}

TEST(RustSharedTest, RustComponentUsesPartitionAlloc) {
  // Verify that PartitionAlloc is consistently used in C++ and Rust.
  auto cpp_allocated_int = std::make_unique<int>();
  SomeStruct* rust_allocated_ptr = allocate_via_rust().into_raw();
  EXPECT_EQ(partition_alloc::IsManagedByPartitionAlloc(
                reinterpret_cast<uintptr_t>(rust_allocated_ptr)),
            partition_alloc::IsManagedByPartitionAlloc(
                reinterpret_cast<uintptr_t>(cpp_allocated_int.get())));
  rust::Box<SomeStruct>::from_raw(rust_allocated_ptr);
}

TEST(RustSharedTest, AllocAligned) {
  alloc_aligned();
}

// TODO(crbug.com/343218479): A rust_shared_library target does not use our
// allocation shim so it is not properly redirected to PartitionAlloc (though is
// happens to use PA on Linux since the std allocator calls malloc). To enable
// this, we need to insert the allocation shim into `rust_shared_library`,
// probably by having the template produce a Rust static library and having
// clang do the linking step.
TEST(RustSharedTest, DISABLED_RustLargeAllocationFailure) {
  // If allocation succeeds, we get true back.
  EXPECT_TRUE(allocate_huge_via_rust(100u, 1u));

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // We only do these tests when using PA, as the system allocator will not fail
  // on large allocations (unless it is really OOM).

  // PartitionAlloc currently limits all allocations to no more than i32::MAX
  // elements, so the allocation will fail. If done through normal malloc(),
  // PA will crash when an allocation fails rather than return null, but Rust
  // can be trusted to handle failure without introducing null derefs so this
  // should fail gracefully.
  size_t max_size = partition_alloc::internal::MaxDirectMapped();
  EXPECT_FALSE(allocate_huge_via_rust(max_size + 1u, 4u));

  // Same as above but with an alignment larger than PartitionAlloc's default
  // alignment, which goes down a different path.
  size_t big_alignment = alignof(std::max_align_t) * 2u;
  EXPECT_FALSE(allocate_huge_via_rust(max_size + 1u, big_alignment));

  // PartitionAlloc will crash if given an alignment larger than this. The
  // allocation hooks handle it gracefully.
  size_t max_alignment = partition_alloc::internal::kMaxSupportedAlignment;
  EXPECT_FALSE(allocate_huge_via_rust(100u, max_alignment * 2u));

#endif
}
