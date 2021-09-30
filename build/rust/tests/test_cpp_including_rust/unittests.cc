// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

extern "C" int32_t add_two_ints_via_rust(int32_t x, int32_t y);
TEST(RustTest, CppCallingIntoRust_BasicFFI) {
  EXPECT_EQ(7, add_two_ints_via_rust(3, 4));
}

extern "C" void* allocate_via_rust();
extern "C" void deallocate_via_rust(void* p);
TEST(RustTest, RustComponentUsesPartitionAlloc) {
  // Verify that PartitionAlloc is consistently used in C++ and Rust.
  auto cpp_allocated_int = std::make_unique<int>();
  void* rust_allocated_ptr = allocate_via_rust();
  EXPECT_EQ(base::IsManagedByPartitionAlloc(rust_allocated_ptr),
            base::IsManagedByPartitionAlloc(cpp_allocated_int.get()));
  deallocate_via_rust(rust_allocated_ptr);
}
