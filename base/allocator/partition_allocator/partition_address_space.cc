// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include <mach-o/dyld.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

#if defined(PA_ENABLE_SHADOW_METADATA)
#include <sys/mman.h>
#endif

namespace partition_alloc::internal {

#if defined(PA_HAS_64_BITS_POINTERS)

namespace {

#if BUILDFLAG(IS_WIN)

#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
bool IsLegacyWindowsVersion() {
  // Use ::RtlGetVersion instead of ::GetVersionEx or helpers from
  // VersionHelpers.h because those alternatives change their behavior depending
  // on whether or not the calling executable has a compatibility manifest
  // resource. It's better for the allocator to not depend on that to decide the
  // pool size.
  // Assume legacy if ::RtlGetVersion is not available or it fails.
  using RtlGetVersion = LONG(WINAPI*)(OSVERSIONINFOEX*);
  const RtlGetVersion rtl_get_version = reinterpret_cast<RtlGetVersion>(
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "RtlGetVersion"));
  if (!rtl_get_version)
    return true;

  OSVERSIONINFOEX version_info = {};
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  if (rtl_get_version(&version_info) != ERROR_SUCCESS)
    return true;

  // Anything prior to Windows 8.1 is considered legacy for the allocator.
  // Windows 8.1 is major 6 with minor 3.
  return version_info.dwMajorVersion < 6 ||
         (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion < 3);
}
#endif  // defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)

PA_NOINLINE void HandlePoolAllocFailureOutOfVASpace() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}

PA_NOINLINE void HandlePoolAllocFailureOutOfCommitCharge() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}
#endif  // BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailure() {
  PA_NO_CODE_FOLDING();
  uint32_t alloc_page_error_code = GetAllocPageErrorCode();
  PA_DEBUG_DATA_ON_STACK("error", static_cast<size_t>(alloc_page_error_code));
  // It's important to easily differentiate these two failures on Windows, so
  // crash with different stacks.
#if BUILDFLAG(IS_WIN)
  if (alloc_page_error_code == ERROR_NOT_ENOUGH_MEMORY) {
    // The error code says NOT_ENOUGH_MEMORY, but since we only do MEM_RESERVE,
    // it must be VA space exhaustion.
    HandlePoolAllocFailureOutOfVASpace();
  } else if (alloc_page_error_code == ERROR_COMMITMENT_LIMIT) {
    // On Windows <8.1, MEM_RESERVE increases commit charge to account for
    // not-yet-committed PTEs needed to cover that VA space, if it was to be
    // committed (see crbug.com/1101421#c16).
    HandlePoolAllocFailureOutOfCommitCharge();
  } else
#endif  // BUILDFLAG(IS_WIN)
  {
    PA_CHECK(false);
  }
}

}  // namespace

alignas(kPartitionCachelineSize)
    PartitionAddressSpace::PoolSetup PartitionAddressSpace::setup_;

#if defined(PA_ENABLE_SHADOW_METADATA)
std::ptrdiff_t PartitionAddressSpace::regular_pool_shadow_offset_ = 0;
std::ptrdiff_t PartitionAddressSpace::brp_pool_shadow_offset_ = 0;
#endif

#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
#if BUILDFLAG(IS_IOS)
namespace {
bool IsIOSTestProcess() {
  // On iOS, only applications with the extended virtual addressing entitlement
  // can use a large address space. Since Earl Grey test runner apps cannot get
  // entitlements, they must use a much smaller pool size.
  uint32_t executable_length = 0;
  _NSGetExecutablePath(NULL, &executable_length);
  PA_DCHECK(executable_length > 0);

  // 'new' cannot be used here, since this function is called during
  // PartitionAddressSpace initialization, at which point 'new' interception
  // is already active. 'malloc' is safe to use, since on Apple platforms,
  // InitializeDefaultAllocatorPartitionRoot() is called before 'malloc'
  // interception is set up.
  char* executable_path = (char*)malloc(executable_length);
  int rv = _NSGetExecutablePath(executable_path, &executable_length);
  PA_DCHECK(!rv);
  size_t executable_path_length =
      std::char_traits<char>::length(executable_path);

  const char kTestProcessSuffix[] = "Runner";
  size_t test_process_suffix_length =
      std::char_traits<char>::length(kTestProcessSuffix);

  if (executable_path_length < test_process_suffix_length)
    return false;

  return !std::char_traits<char>::compare(
      executable_path + (executable_path_length - test_process_suffix_length),
      kTestProcessSuffix, test_process_suffix_length);
}
}  // namespace

PA_ALWAYS_INLINE size_t PartitionAddressSpace::RegularPoolSize() {
  return IsIOSTestProcess() ? kRegularPoolSizeForIOSTestProcess
                            : kRegularPoolSize;
}
PA_ALWAYS_INLINE size_t PartitionAddressSpace::BRPPoolSize() {
  return IsIOSTestProcess() ? kBRPPoolSizeForIOSTestProcess : kBRPPoolSize;
}
#else
PA_ALWAYS_INLINE size_t PartitionAddressSpace::RegularPoolSize() {
  return IsLegacyWindowsVersion() ? kRegularPoolSizeForLegacyWindows
                                  : kRegularPoolSize;
}
PA_ALWAYS_INLINE size_t PartitionAddressSpace::BRPPoolSize() {
  return IsLegacyWindowsVersion() ? kBRPPoolSizeForLegacyWindows : kBRPPoolSize;
}
#endif  // BUILDFLAG(IS_IOS)
#endif  // defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)

void PartitionAddressSpace::Init() {
  if (IsInitialized())
    return;

  size_t regular_pool_size = RegularPoolSize();
#if defined(PA_ENABLE_SHADOW_METADATA)
  int regular_pool_fd = memfd_create("/regular_pool", MFD_CLOEXEC);
#else
  int regular_pool_fd = -1;
#endif
  setup_.regular_pool_base_address_ =
      AllocPages(regular_pool_size, regular_pool_size,
                 PageAccessibilityConfiguration::kInaccessible,
                 PageTag::kPartitionAlloc, regular_pool_fd);
  if (!setup_.regular_pool_base_address_)
    HandlePoolAllocFailure();
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
  setup_.regular_pool_base_mask_ = ~(regular_pool_size - 1);
#endif
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (regular_pool_size - 1)));
  AddressPoolManager::GetInstance().Add(
      kRegularPoolHandle, setup_.regular_pool_base_address_, regular_pool_size);
  PA_DCHECK(!IsInRegularPool(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_ +
                            regular_pool_size - 1));
  PA_DCHECK(
      !IsInRegularPool(setup_.regular_pool_base_address_ + regular_pool_size));

  size_t brp_pool_size = BRPPoolSize();
#if defined(PA_ENABLE_SHADOW_METADATA)
  int brp_pool_fd = memfd_create("/brp_pool", MFD_CLOEXEC);
#else
  int brp_pool_fd = -1;
#endif
  // Reserve an extra allocation granularity unit before the BRP pool, but keep
  // the pool aligned at BRPPoolSize(). A pointer immediately past an allocation
  // is a valid pointer, and having a "forbidden zone" before the BRP pool
  // prevents such a pointer from "sneaking into" the pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  uintptr_t base_address = AllocPagesWithAlignOffset(
      0, brp_pool_size + kForbiddenZoneSize, brp_pool_size,
      brp_pool_size - kForbiddenZoneSize,
      PageAccessibilityConfiguration::kInaccessible, PageTag::kPartitionAlloc,
      brp_pool_fd);
  if (!base_address)
    HandlePoolAllocFailure();
  setup_.brp_pool_base_address_ = base_address + kForbiddenZoneSize;
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
  setup_.brp_pool_base_mask_ = ~(brp_pool_size - 1);
#endif
  PA_DCHECK(!(setup_.brp_pool_base_address_ & (brp_pool_size - 1)));
  AddressPoolManager::GetInstance().Add(
      kBRPPoolHandle, setup_.brp_pool_base_address_, brp_pool_size);
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_ + brp_pool_size - 1));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ + brp_pool_size));

#if PA_STARSCAN_USE_CARD_TABLE
  // Reserve memory for PCScan quarantine card table.
  uintptr_t requested_address = setup_.regular_pool_base_address_;
  uintptr_t actual_address = AddressPoolManager::GetInstance().Reserve(
      kRegularPoolHandle, requested_address, kSuperPageSize);
  PA_CHECK(requested_address == actual_address)
      << "QuarantineCardTable is required to be allocated at the beginning of "
         "the regular pool";
#endif  // PA_STARSCAN_USE_CARD_TABLE

#if defined(PA_ENABLE_SHADOW_METADATA)
  // Reserve memory for the shadow pools.
  uintptr_t regular_pool_shadow_address =
      AllocPages(regular_pool_size, regular_pool_size,
                 PageAccessibilityConfiguration::kInaccessible,
                 PageTag::kPartitionAlloc, regular_pool_fd);
  regular_pool_shadow_offset_ =
      regular_pool_shadow_address - setup_.regular_pool_base_address_;

  uintptr_t brp_pool_shadow_address = AllocPagesWithAlignOffset(
      0, brp_pool_size + kForbiddenZoneSize, brp_pool_size,
      brp_pool_size - kForbiddenZoneSize,
      PageAccessibilityConfiguration::kInaccessible, PageTag::kPartitionAlloc,
      brp_pool_fd);
  brp_pool_shadow_offset_ =
      brp_pool_shadow_address - setup_.brp_pool_base_address_;
#endif
}

void PartitionAddressSpace::InitConfigurablePool(uintptr_t pool_base,
                                                 size_t size) {
  // The ConfigurablePool must only be initialized once.
  PA_CHECK(!IsConfigurablePoolInitialized());

  // The other Pools must be initialized first.
  Init();

  PA_CHECK(pool_base);
  PA_CHECK(size <= kConfigurablePoolMaxSize);
  PA_CHECK(size >= kConfigurablePoolMinSize);
  PA_CHECK(base::bits::IsPowerOfTwo(size));
  PA_CHECK(pool_base % size == 0);

  setup_.configurable_pool_base_address_ = pool_base;
  setup_.configurable_pool_base_mask_ = ~(size - 1);

  AddressPoolManager::GetInstance().Add(
      kConfigurablePoolHandle, setup_.configurable_pool_base_address_, size);
}

void PartitionAddressSpace::UninitForTesting() {
  FreePages(setup_.regular_pool_base_address_, RegularPoolSize());
  // For BRP pool, the allocation region includes a "forbidden zone" before the
  // pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  FreePages(setup_.brp_pool_base_address_ - kForbiddenZoneSize,
            BRPPoolSize() + kForbiddenZoneSize);
  // Do not free pages for the configurable pool, because its memory is owned
  // by someone else, but deinitialize it nonetheless.
  setup_.regular_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.brp_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
  AddressPoolManager::GetInstance().ResetForTesting();
}

void PartitionAddressSpace::UninitConfigurablePoolForTesting() {
  AddressPoolManager::GetInstance().Remove(kConfigurablePoolHandle);
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
}

#if BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)

PageCharacteristics page_characteristics;

#endif  // BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace partition_alloc::internal
