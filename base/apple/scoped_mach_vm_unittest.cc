// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/scoped_mach_vm.h"

#include <mach/mach.h>

#include "base/memory/page_size.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note: This test CANNOT be run multiple times within the same process (e.g.
// with --gtest_repeat). Allocating and deallocating in quick succession, even
// with different sizes, will typically result in the kernel returning the same
// address. If the allocation pattern is small->large->small, the second small
// allocation will report being part of the previously-deallocated large region.
// That will cause the GetRegionInfo() expectations to fail.

namespace base::apple {
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
  vm_size_t size = base::GetPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper(address, size);
  EXPECT_EQ(address, scoper.address());
  EXPECT_EQ(size, scoper.size());

  // Test the initial region. In some cases on some platforms (macOS 13 on
  // Intel, for example), Darwin may combine the requested allocation with
  // an existing one. As a result, the allocated region may live in a
  // larger region. Therefore, when we GetRegionInfo(), we want to check
  // that our original region is a subset of (region_address, region_size)
  // rather than being exactly equal to it.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + size, region_address + region_size);

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
  vm_size_t size = base::GetPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper(address, size);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + size, region_address + region_size);

  scoper.reset();

  // After deallocation, the kernel will return the next highest address.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_LT(address, region_address);
}

TEST(ScopedMachVMTest, ResetSmallerAddress) {
  vm_address_t address;
  vm_size_t size = 2 * base::GetPageSize();
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper(address, base::GetPageSize());

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_EQ(address, region_address);
  EXPECT_EQ(2u * base::GetPageSize(), region_size);

  // This will free address..base::GetPageSize() that is currently in the
  // scoper.
  scoper.reset(address + base::GetPageSize(), base::GetPageSize());

  // Verify that the region is now only one page.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(address + base::GetPageSize(), region_address);
  EXPECT_EQ(1u * base::GetPageSize(), region_size);
}

TEST(ScopedMachVMTest, ResetLargerAddressAndSize) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kTwoPages = 2 * kOnePage;
  const vm_size_t kThreePages = 3 * kOnePage;

  vm_address_t address;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kThreePages, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + kThreePages, region_address + region_size);

  ScopedMachVM scoper(address + kTwoPages, kOnePage);
  // Expand the region to be larger.
  scoper.reset(address, kThreePages);

  // Verify that the region is still three pages.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + kThreePages, region_address + region_size);
}

TEST(ScopedMachVMTest, ResetLargerAddress) {
  const vm_size_t kThreePages = 3 * base::GetPageSize();
  const vm_size_t kSixPages = 2 * kThreePages;

  vm_address_t address;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSixPages, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_EQ(KERN_SUCCESS, kr);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + kSixPages, region_address + region_size);

  ScopedMachVM scoper(address + kThreePages, kThreePages);

  // Shift the region by three pages; the last three pages should be
  // deallocated, while keeping the first three.
  scoper.reset(address, kThreePages);

  // Verify that the region is just three pages.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + kThreePages, region_address + region_size);
}

TEST(ScopedMachVMTest, ResetUnaligned) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kTwoPages = 2 * kOnePage;

  vm_address_t address;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kTwoPages, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper;

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + kTwoPages, region_address + region_size);

  // Initialize with unaligned size.
  scoper.reset_unaligned(address + kOnePage, kOnePage - 3);
  // Reset with another unaligned size.
  scoper.reset_unaligned(address + kOnePage, kOnePage - 11);

  // The entire unaligned page gets deallocated.
  region_address = address;
  GetRegionInfo(&region_address, &region_size);
  EXPECT_GE(address, region_address);
  EXPECT_LE(address + kOnePage, region_address + region_size);

  // Reset with the remaining page.
  scoper.reset_unaligned(address, base::GetPageSize());
}

#if DCHECK_IS_ON()

TEST(ScopedMachVMTest, ResetMustBeAligned) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kTwoPages = 2 * kOnePage;

  vm_address_t address;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kTwoPages, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr);

  ScopedMachVM scoper;
  EXPECT_DCHECK_DEATH(scoper.reset(address, kOnePage + 1));
}

#endif  // DCHECK_IS_ON()

}  // namespace
}  // namespace base::apple
