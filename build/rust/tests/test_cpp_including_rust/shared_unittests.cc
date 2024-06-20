// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "build/buildflag.h"
#include "build/rust/tests/test_rust_shared_library/src/lib.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "base/allocator/partition_allocator/src/partition_alloc/partition_address_space.h"
#else
#include "base/allocator/partition_allocator/src/partition_alloc/address_pool_manager_bitmap.h"
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
