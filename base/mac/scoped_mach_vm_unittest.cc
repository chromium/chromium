// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_mach_vm.h"

#include <mach/mach.h>

#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note: This test CANNOT be run multiple times within the same process (e.g.
// with --gtest_repeat). Allocating and deallocating in quick succession, even
// with different sizes, will typically result in the kernel returning the same
// address. If the allocation pattern is small->large->small, the second small
// allocation will report being part of the previously-deallocated large region.
// That will cause the GetRegionInfo() expectations to fail.

namespace base {
namespace mac {
namespace {

void GetRegionInfo(vm_address_t* region_address, vm_size_t* region_size) {
  vm_region_basic_info_64 region_info;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object;
  kern_return_t kr = vm_region_64(
      mach_task_self(), region_address, region_size, VM_REGION_BASIC_INFO_64,
      reinterpret_cast<vm_region_info_t>(&region_info), &count, &object);
  EXPECT_EQ(KERN_SUCCESS, kr);
}

TEST(ScopedMachVMTest, Basic) {
  vm_address_t address;
  vm_size_t size = base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper(address, size);
  EXPECT_EQ(address, scoper.address());
  EXPECT_EQ(size, scoper.size());

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(1u * base::SystemPageSize(), region_size);

  {
    ScopedMachVM scoper2;
    EXPECT_EQ(0u, scoper2.address());
    EXPECT_EQ(0u, scoper2.size());

    scoper.swap(scoper2);

    EXPECT_EQ(address, scoper2.address());
    EXPECT_EQ(size, scoper2.size());

    EXPECT_EQ(0u, scoper.address());
    EXPECT_EQ(0u, scoper.size());
  }

  // After deallocation, the kernel will return the next highest address.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_LT(address, region_address);
}

TEST(ScopedMachVMTest, Reset) {
  vm_address_t address;
  vm_size_t size = base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper(address, size);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(1u * base::SystemPageSize(), region_size);

  scoper.reset();

  // After deallocation, the kernel will return the next highest address.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_LT(address, region_address);
}

TEST(ScopedMachVMTest, ResetSmallerAddress) {
  vm_address_t address;
  vm_size_t size = 2 * base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper(address, base::SystemPageSize());

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(2u * base::SystemPageSize(), region_size);

  // This will free address..base::SystemPageSize() that is currently in the
  // scoper.
  scoper.reset(address + base::SystemPageSize(), base::SystemPageSize());

  // Verify that the region is now only one page.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(address + base::SystemPageSize(), region_address);
  EXPECT_EQ(1u * base::SystemPageSize(), region_size);
}

TEST(ScopedMachVMTest, ResetLargerAddressAndSize) {
  vm_address_t address;
  vm_size_t size = 3 * base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(3u * base::SystemPageSize(), region_size);

  ScopedMachVM scoper(address + 2 * base::SystemPageSize(),
                      base::SystemPageSize());
  // Expand the region to be larger.
  scoper.reset(address, size);

  // Verify that the region is still three pages.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(3u * base::SystemPageSize(), region_size);
}

TEST(ScopedMachVMTest, ResetLargerAddress) {
  vm_address_t address;
  vm_size_t size = 6 * base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(6u * base::SystemPageSize(), region_size);

  ScopedMachVM scoper(address + 3 * base::SystemPageSize(),
                      3 * base::SystemPageSize());

  // Shift the region by three pages; the last three pages should be
  // deallocated, while keeping the first three.
  scoper.reset(address, 3 * base::SystemPageSize());

  // Verify that the region is just three pages.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(3u * base::SystemPageSize(), region_size);
}

TEST(ScopedMachVMTest, ResetUnaligned) {
  vm_address_t address;
  vm_size_t size = 2 * base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper;

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(2u * base::SystemPageSize(), region_size);

  // Initialize with unaligned size.
  scoper.reset_unaligned(address + base::SystemPageSize(),
                         base::SystemPageSize() - 3);
  // Reset with another unaligned size.
  scoper.reset_unaligned(address + base::SystemPageSize(),
                         base::SystemPageSize() - 11);

  // The entire unaligned page gets deallocated.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(1u * base::SystemPageSize(), region_size);

  // Reset with the remaining page.
  scoper.reset_unaligned(address, base::SystemPageSize());
}

#if DCHECK_IS_ON()

TEST(ScopedMachVMTest, ResetMustBeAligned) {
  vm_address_t address;
  vm_size_t size = 2 * base::SystemPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper;
  EXPECT_DCHECK_DEATH(scoper.reset(address, base::SystemPageSize() + 1));
}

#endif  // DCHECK_IS_ON()

}  // namespace
}  // namespace mac
}  // namespace base
