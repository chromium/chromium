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

// The OS may try to combine an allocation with an immediately adjacent one if
// all of their properties are identical, which can make some of these tests
// fail when they find regions larger than expected. To avoid that problem, use
// a custom tag on allocations, where the tag is not likely to be used anywhere
// else. That makes it unlikely that an allocation made by these tests can be
// combined with another adjacent allocation.
constexpr int kVmAllocateTagNumber = 248;
static_assert(kVmAllocateTagNumber >= VM_MEMORY_APPLICATION_SPECIFIC_1);
static_assert(kVmAllocateTagNumber <= VM_MEMORY_APPLICATION_SPECIFIC_16);
constexpr int kVmAllocateTag = VM_MAKE_TAG(kVmAllocateTagNumber);

bool GetRegionInfo(vm_address_t* region_address, vm_size_t* region_size) {
  vm_region_basic_info_64 region_info;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object;
  kern_return_t kr = vm_region_64(
      mach_task_self(), region_address, region_size, VM_REGION_BASIC_INFO_64,
      reinterpret_cast<vm_region_info_t>(&region_info), &count, &object);
  EXPECT_EQ(kr, KERN_SUCCESS);
  return kr == KERN_SUCCESS;
}

TEST(ScopedMachVMTest, Basic) {
  const vm_size_t kOnePage = base::GetPageSize();

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kOnePage,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  ScopedMachVM scoper(address, kOnePage);
  EXPECT_EQ(scoper.address(), address);
  EXPECT_EQ(scoper.size(), kOnePage);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kOnePage);
  }

  {
    ScopedMachVM scoper2;
    EXPECT_EQ(scoper2.address(), 0u);
    EXPECT_EQ(scoper2.size(), 0u);

    scoper.swap(scoper2);

    EXPECT_EQ(scoper2.address(), address);
    EXPECT_EQ(scoper2.size(), kOnePage);

    EXPECT_EQ(scoper.address(), 0u);
    EXPECT_EQ(scoper.size(), 0u);
  }

  // After deallocation, the kernel will return the next highest address.
  region_address = address;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_GE(region_address, address + kOnePage);
  }
}

TEST(ScopedMachVMTest, Reset) {
  const vm_size_t kOnePage = base::GetPageSize();

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kOnePage,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  ScopedMachVM scoper(address, kOnePage);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kOnePage);
  }

  scoper.reset();

  // After deallocation, the kernel will return the next highest address.
  region_address = address;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_GE(region_address, address + kOnePage);
  }
}

TEST(ScopedMachVMTest, ResetSmallerAddress) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kThreePages = 3 * kOnePage;

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kThreePages,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  ScopedMachVM scoper(address, kThreePages);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kThreePages);
  }

  // This will free pages 0 and 2 originally supervised by the scoper, leaving
  // just original page 1.
  scoper.reset(address + kOnePage, kOnePage);

  // Verify that the region is now only one page.
  region_address = address;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address + kOnePage);
    EXPECT_EQ(region_size, kOnePage);
  }
}

TEST(ScopedMachVMTest, ResetLargerAddressAndSize) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kTwoPages = 2 * kOnePage;
  const vm_size_t kThreePages = 3 * kOnePage;

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kThreePages,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kThreePages);
  }

  ScopedMachVM scoper(address + kTwoPages, kOnePage);
  // Expand the region to be larger.
  scoper.reset(address, kThreePages);

  // Verify that the region is still three pages.
  region_address = address;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kThreePages);
  }
}

TEST(ScopedMachVMTest, ResetLargerAddress) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kThreePages = 3 * kOnePage;
  const vm_size_t kSixPages = 6 * kOnePage;

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kSixPages,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kSixPages);
  }

  ScopedMachVM scoper(address + kThreePages, kThreePages);

  // Shift the region by three pages; the last three pages should be
  // deallocated, while keeping the first three.
  scoper.reset(address, kThreePages);

  // Verify that the region is just three pages.
  region_address = address;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kThreePages);
  }
}

TEST(ScopedMachVMTest, ResetUnaligned) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kTwoPages = 2 * kOnePage;

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kTwoPages,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  ScopedMachVM scoper;

  // Test the initial region.
  vm_address_t region_address = address;
  vm_size_t region_size;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kTwoPages);
  }

  // Initialize with unaligned size.
  scoper.reset_unaligned(address + kOnePage, kOnePage - 3);
  // Reset with another unaligned size.
  scoper.reset_unaligned(address + kOnePage, kOnePage - 11);

  // The entire unaligned page gets deallocated.
  region_address = address;
  if (GetRegionInfo(&region_address, &region_size)) {
    EXPECT_EQ(region_address, address);
    EXPECT_EQ(region_size, kOnePage);
  }

  // Reset with the remaining page.
  scoper.reset_unaligned(address, kOnePage);
}

#if DCHECK_IS_ON()

TEST(ScopedMachVMTest, ResetMustBeAligned) {
  const vm_size_t kOnePage = base::GetPageSize();
  const vm_size_t kTwoPages = 2 * kOnePage;

  vm_address_t address;
  kern_return_t kr = vm_allocate(mach_task_self(), &address, kTwoPages,
                                 VM_FLAGS_ANYWHERE | kVmAllocateTag);
  ASSERT_EQ(kr, KERN_SUCCESS);

  ScopedMachVM scoper;
  EXPECT_DCHECK_DEATH(scoper.reset(address, kOnePage + 1));
}

#endif  // DCHECK_IS_ON()

}  // namespace
}  // namespace base::apple
