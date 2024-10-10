// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_address_space.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/compressed_pointer.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if PA_BUILDFLAG(IS_IOS)
#include <mach-o/dyld.h>
#endif

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // PA_BUILDFLAG(IS_WIN)

#if PA_CONFIG(ENABLE_SHADOW_METADATA) || PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include <sys/mman.h>
#endif

namespace partition_alloc::internal {

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace {

#if PA_BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailureOutOfVASpace() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}

PA_NOINLINE void HandlePoolAllocFailureOutOfCommitCharge() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}
#endif  // PA_BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailure() {
  PA_NO_CODE_FOLDING();
  uint32_t alloc_page_error_code = GetAllocPageErrorCode();
  PA_DEBUG_DATA_ON_STACK("error", static_cast<size_t>(alloc_page_error_code));
  // It's important to easily differentiate these two failures on Windows, so
  // crash with different stacks.
#if PA_BUILDFLAG(IS_WIN)
  if (alloc_page_error_code == ERROR_NOT_ENOUGH_MEMORY) {
    // The error code says NOT_ENOUGH_MEMORY, but since we only do MEM_RESERVE,
    // it must be VA space exhaustion.
    HandlePoolAllocFailureOutOfVASpace();
  } else if (alloc_page_error_code == ERROR_COMMITMENT_LIMIT ||
             alloc_page_error_code == ERROR_COMMITMENT_MINIMUM) {
    // Should not happen, since as of Windows 8.1+, reserving address space
    // should not be charged against the commit limit, aside from a very small
    // amount per 64kiB block. Keep this path anyway, to check in crash reports.
    HandlePoolAllocFailureOutOfCommitCharge();
  } else
#endif  // PA_BUILDFLAG(IS_WIN)
  {
    PA_CHECK(false);
  }
}

}  // namespace

PA_CONSTINIT PartitionAddressSpace::PoolSetup PartitionAddressSpace::setup_;

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
std::ptrdiff_t PartitionAddressSpace::regular_pool_shadow_offset_ = 0;
std::ptrdiff_t PartitionAddressSpace::brp_pool_shadow_offset_ = 0;
std::ptrdiff_t PartitionAddressSpace::configurable_pool_shadow_offset_ = 0;

// File descriptors for shared mappings.
int PartitionAddressSpace::regular_pool_fd_ = -1;
int PartitionAddressSpace::brp_pool_fd_ = -1;
int PartitionAddressSpace::configurable_pool_fd_ = -1;

uintptr_t PartitionAddressSpace::pool_shadow_address_ =
    PartitionAddressSpace::kUninitializedPoolBaseAddress;
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#if !PA_BUILDFLAG(IS_IOS)
#error Dynamic pool size is only supported on iOS.
#endif

bool PartitionAddressSpace::IsIOSTestProcess() {
  // On iOS, only applications with the extended virtual addressing entitlement
  // can use a large address space. Since Earl Grey test runner apps cannot get
  // entitlements, they must use a much smaller pool size. Similarly,
  // integration tests for ChromeWebView end up with two PartitionRoots since
  // both the integration tests and ChromeWebView have a copy of base/. Even
  // with the entitlement, there is insufficient address space for two
  // PartitionRoots, so a smaller pool size is needed.

  // Use a fixed buffer size to avoid allocation inside the allocator.
  constexpr size_t path_buffer_size = 8192;
  char executable_path[path_buffer_size];

  uint32_t executable_length = path_buffer_size;
  int rv = _NSGetExecutablePath(executable_path, &executable_length);
  PA_CHECK(!rv);
  size_t executable_path_length =
      std::char_traits<char>::length(executable_path);

  auto has_suffix = [&](const char* suffix) -> bool {
    size_t suffix_length = std::char_traits<char>::length(suffix);
    if (executable_path_length < suffix_length) {
      return false;
    }
    return std::char_traits<char>::compare(
               executable_path + (executable_path_length - suffix_length),
               suffix, suffix_length) == 0;
  };

  return has_suffix("Runner") || has_suffix("ios_web_view_inttests");
}
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
size_t PartitionAddressSpace::RegularPoolShadowSize() {
  return RegularPoolSize();
}

size_t PartitionAddressSpace::BRPPoolShadowSize() {
  return BRPPoolSize();
}

size_t PartitionAddressSpace::ConfigurablePoolShadowSize() {
  return kConfigurablePoolMaxSize;
}
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

void PartitionAddressSpace::Init() {
  if (IsInitialized()) {
    return;
  }

  const size_t regular_pool_size = RegularPoolSize();
  const size_t brp_pool_size = BRPPoolSize();

#if PA_BUILDFLAG(GLUE_CORE_POOLS)
  // Gluing core pools (regular & BRP) makes sense only when both pools are of
  // the same size. This the only way we can check belonging to either of the
  // two with a single bitmask operation.
  PA_CHECK(regular_pool_size == brp_pool_size);

  // TODO(crbug.com/40238514): Support PA_ENABLE_SHADOW_METADATA.
  int pools_fd = -1;

  size_t glued_pool_sizes = regular_pool_size * 2;
  // Note, BRP pool requires to be preceded by a "forbidden zone", which is
  // conveniently taken care of by the last guard page of the regular pool.
  setup_.regular_pool_base_address_ =
      AllocPages(glued_pool_sizes, glued_pool_sizes,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc, pools_fd);
#if PA_BUILDFLAG(IS_ANDROID)
  // On Android, Adreno-GSL library fails to mmap if we snatch address
  // 0x400000000. Find a different address instead.
  if (setup_.regular_pool_base_address_ == 0x400000000) {
    uintptr_t new_base_address =
        AllocPages(glued_pool_sizes, glued_pool_sizes,
                   PageAccessibilityConfiguration(
                       PageAccessibilityConfiguration::kInaccessible),
                   PageTag::kPartitionAlloc, pools_fd);
    FreePages(setup_.regular_pool_base_address_, glued_pool_sizes);
    setup_.regular_pool_base_address_ = new_base_address;
  }
#endif  // PA_BUILDFLAG(IS_ANDROID)
  if (!setup_.regular_pool_base_address_) {
    HandlePoolAllocFailure();
  }
  setup_.brp_pool_base_address_ =
      setup_.regular_pool_base_address_ + regular_pool_size;
#else  // PA_BUILDFLAG(GLUE_CORE_POOLS)
  setup_.regular_pool_base_address_ =
      AllocPages(regular_pool_size, regular_pool_size,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  if (!setup_.regular_pool_base_address_) {
    HandlePoolAllocFailure();
  }

  // Reserve an extra allocation granularity unit before the BRP pool, but keep
  // the pool aligned at BRPPoolSize(). A pointer immediately past an allocation
  // is a valid pointer, and having a "forbidden zone" before the BRP pool
  // prevents such a pointer from "sneaking into" the pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  uintptr_t base_address = AllocPagesWithAlignOffset(
      0, brp_pool_size + kForbiddenZoneSize, brp_pool_size,
      brp_pool_size - kForbiddenZoneSize,
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kInaccessible),
      PageTag::kPartitionAlloc, -1);
  if (!base_address) {
    HandlePoolAllocFailure();
  }
  setup_.brp_pool_base_address_ = base_address + kForbiddenZoneSize;
#endif  // PA_BUILDFLAG(GLUE_CORE_POOLS)

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  setup_.regular_pool_base_mask_ = ~(regular_pool_size - 1);
  setup_.brp_pool_base_mask_ = ~(brp_pool_size - 1);
#if PA_BUILDFLAG(GLUE_CORE_POOLS)
  // When PA_GLUE_CORE_POOLS is on, the BRP pool is placed at the end of the
  // regular pool, effectively forming one virtual pool of a twice bigger
  // size. Adjust the mask appropriately.
  setup_.core_pools_base_mask_ = setup_.regular_pool_base_mask_ << 1;
  PA_DCHECK(setup_.core_pools_base_mask_ == (setup_.brp_pool_base_mask_ << 1));
#endif
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

  AddressPoolManager::GetInstance().Add(
      kRegularPoolHandle, setup_.regular_pool_base_address_, regular_pool_size);
  AddressPoolManager::GetInstance().Add(
      kBRPPoolHandle, setup_.brp_pool_base_address_, brp_pool_size);

  // Sanity check pool alignment.
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (regular_pool_size - 1)));
  PA_DCHECK(!(setup_.brp_pool_base_address_ & (brp_pool_size - 1)));
#if PA_BUILDFLAG(GLUE_CORE_POOLS)
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (glued_pool_sizes - 1)));
#endif

  // Sanity check pool belonging.
  PA_DCHECK(!IsInRegularPool(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_ +
                            regular_pool_size - 1));
  PA_DCHECK(
      !IsInRegularPool(setup_.regular_pool_base_address_ + regular_pool_size));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_ + brp_pool_size - 1));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ + brp_pool_size));
#if PA_BUILDFLAG(GLUE_CORE_POOLS)
  PA_DCHECK(!IsInCorePools(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInCorePools(setup_.regular_pool_base_address_));
  PA_DCHECK(
      IsInCorePools(setup_.regular_pool_base_address_ + regular_pool_size - 1));
  PA_DCHECK(
      IsInCorePools(setup_.regular_pool_base_address_ + regular_pool_size));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_ + brp_pool_size - 1));
  PA_DCHECK(!IsInCorePools(setup_.brp_pool_base_address_ + brp_pool_size));
#endif  // PA_BUILDFLAG(GLUE_CORE_POOLS)

#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  CompressedPointerBaseGlobal::SetBase(setup_.regular_pool_base_address_);
#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
}

void PartitionAddressSpace::InitConfigurablePool(uintptr_t pool_base,
                                                 size_t size) {
  // The ConfigurablePool must only be initialized once.
  PA_CHECK(!IsConfigurablePoolInitialized());

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // It's possible that the thread isolated pool has been initialized first, in
  // which case the setup_ memory has been made read-only. Remove the protection
  // temporarily.
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
  }
#endif

  PA_CHECK(pool_base);
  PA_CHECK(size <= kConfigurablePoolMaxSize);
  PA_CHECK(size >= kConfigurablePoolMinSize);
  PA_CHECK(base::bits::HasSingleBit(size));
  PA_CHECK(pool_base % size == 0);

  setup_.configurable_pool_base_address_ = pool_base;
  setup_.configurable_pool_base_mask_ = ~(size - 1);

  AddressPoolManager::GetInstance().Add(
      kConfigurablePoolHandle, setup_.configurable_pool_base_address_, size);

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Put the metadata protection back in place.
  if (IsThreadIsolatedPoolInitialized()) {
    WriteProtectThreadIsolatedGlobals(setup_.thread_isolation_);
  }
#endif
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAddressSpace::InitThreadIsolatedPool(
    ThreadIsolationOption thread_isolation) {
  // The ThreadIsolated pool can't be initialized with conflicting settings.
  if (IsThreadIsolatedPoolInitialized()) {
    PA_CHECK(setup_.thread_isolation_ == thread_isolation);
    return;
  }

  size_t pool_size = ThreadIsolatedPoolSize();
  setup_.thread_isolated_pool_base_address_ =
      AllocPages(pool_size, pool_size,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  if (!setup_.thread_isolated_pool_base_address_) {
    HandlePoolAllocFailure();
  }

  PA_DCHECK(!(setup_.thread_isolated_pool_base_address_ & (pool_size - 1)));
  setup_.thread_isolation_ = thread_isolation;
  AddressPoolManager::GetInstance().Add(
      kThreadIsolatedPoolHandle, setup_.thread_isolated_pool_base_address_,
      pool_size);

  PA_DCHECK(
      !IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_ - 1));
  PA_DCHECK(IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_));
  PA_DCHECK(IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_ +
                                   pool_size - 1));
  PA_DCHECK(!IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_ +
                                    pool_size));

  // TODO(crbug.com/40238514): support PA_ENABLE_SHADOW_METADATA
}
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

void PartitionAddressSpace::UninitForTesting() {
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  UninitThreadIsolatedPoolForTesting();  // IN-TEST
#endif
#if PA_BUILDFLAG(GLUE_CORE_POOLS)
  // The core pools (regular & BRP) were allocated using a single allocation of
  // double size.
  FreePages(setup_.regular_pool_base_address_, 2 * RegularPoolSize());
#else   // PA_BUILDFLAG(GLUE_CORE_POOLS)
  FreePages(setup_.regular_pool_base_address_, RegularPoolSize());
  // For BRP pool, the allocation region includes a "forbidden zone" before the
  // pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  FreePages(setup_.brp_pool_base_address_ - kForbiddenZoneSize,
            BRPPoolSize() + kForbiddenZoneSize);
#endif  // PA_BUILDFLAG(GLUE_CORE_POOLS)
  // Do not free pages for the configurable pool, because its memory is owned
  // by someone else, but deinitialize it nonetheless.
  setup_.regular_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.brp_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
  AddressPoolManager::GetInstance().ResetForTesting();
#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  CompressedPointerBaseGlobal::ResetBaseForTesting();
#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
}

void PartitionAddressSpace::UninitConfigurablePoolForTesting() {
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // It's possible that the thread isolated pool has been initialized first, in
  // which case the setup_ memory has been made read-only. Remove the protection
  // temporarily.
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
  }
#endif
  AddressPoolManager::GetInstance().Remove(kConfigurablePoolHandle);
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Put the metadata protection back in place.
  if (IsThreadIsolatedPoolInitialized()) {
    WriteProtectThreadIsolatedGlobals(setup_.thread_isolation_);
  }
#endif
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAddressSpace::UninitThreadIsolatedPoolForTesting() {
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    ThreadIsolationSettings::settings.enabled = false;
#endif

    FreePages(setup_.thread_isolated_pool_base_address_,
              ThreadIsolatedPoolSize());
    AddressPoolManager::GetInstance().Remove(kThreadIsolatedPoolHandle);
    setup_.thread_isolated_pool_base_address_ = kUninitializedPoolBaseAddress;
    setup_.thread_isolation_.enabled = false;
  }
}
#endif

#if PA_CONFIG(ENABLE_SHADOW_METADATA)

namespace {

int CreateAnonymousFileForMapping([[maybe_unused]] const char* name,
                                  [[maybe_unused]] size_t size) {
  int fd = -1;
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40238514): if memfd_secret() is available, try
  // memfd_secret() first.
  fd = memfd_create(name, MFD_CLOEXEC);
  PA_CHECK(0 == ftruncate(fd, size));
#else
  // Not implemented yet.
  PA_NOTREACHED();
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  return fd;
}

}  // namespace

void PartitionAddressSpace::InitShadowMetadata(PoolHandleMask mask) {
  // Set up an address space only once.
  if (pool_shadow_address_ == kUninitializedPoolBaseAddress) {
    // Reserve 1 address space for all pools.
    const size_t shadow_pool_size =
        std::max(ConfigurablePoolShadowSize(),
                 std::max(RegularPoolShadowSize(), BRPPoolShadowSize()));

    // Reserve virtual address space for the shadow pool.
    uintptr_t pool_shadow_address =
        AllocPages(shadow_pool_size, PageAllocationGranularity(),
                   PageAccessibilityConfiguration(
                       PageAccessibilityConfiguration::kInaccessible),
                   PageTag::kPartitionAlloc);
    if (!pool_shadow_address) {
      HandlePoolAllocFailure();
    }

    pool_shadow_address_ = pool_shadow_address;
  }

  // Set up a memory file for the given pool, and init |offset|.
  if (ContainsFlags(mask, PoolHandleMask::kConfigurable)) {
    if (configurable_pool_fd_ == -1) {
      PA_DCHECK(pool_shadow_address_);
      PA_DCHECK(configurable_pool_shadow_offset_ == 0);
      configurable_pool_fd_ = CreateAnonymousFileForMapping(
          "configurable_pool_shadow", ConfigurablePoolShadowSize());
      configurable_pool_shadow_offset_ =
          pool_shadow_address_ - ConfigurablePoolBase() +
          SystemPageSize() * kSystemPageOffsetOfConfigurablePoolShadow;
    }
  }
  if (ContainsFlags(mask, PoolHandleMask::kBRP)) {
    if (brp_pool_fd_ == -1) {
      PA_DCHECK(pool_shadow_address_);
      PA_DCHECK(brp_pool_shadow_offset_ == 0);
      brp_pool_fd_ =
          CreateAnonymousFileForMapping("brp_pool_shadow", BRPPoolShadowSize());
      brp_pool_shadow_offset_ =
          pool_shadow_address_ - BRPPoolBase() +
          SystemPageSize() * kSystemPageOffsetOfBRPPoolShadow;
    }
  }
  if (ContainsFlags(mask, PoolHandleMask::kRegular)) {
    if (regular_pool_fd_ == -1) {
      PA_DCHECK(pool_shadow_address_);
      PA_DCHECK(regular_pool_shadow_offset_ == 0);
      regular_pool_fd_ = CreateAnonymousFileForMapping("regular_pool_shadow",
                                                       RegularPoolShadowSize());
      regular_pool_shadow_offset_ =
          pool_shadow_address_ - RegularPoolBase() +
          SystemPageSize() * kSystemPageOffsetOfRegularPoolShadow;
    }
  }
}

// Share a read-only metadata inside the given SuperPage with its writable
// metadata.
void PartitionAddressSpace::MapMetadata(uintptr_t super_page,
                                        bool copy_metadata) {
  PA_DCHECK(pool_shadow_address_);
  PA_DCHECK(0u == (super_page & kSuperPageOffsetMask));
  std::ptrdiff_t offset;
  int pool_fd = -1;
  uintptr_t base_address;

  if (IsInRegularPool(super_page)) {
    pool_fd = regular_pool_fd_;
    offset = regular_pool_shadow_offset_;
    base_address = RegularPoolBase();
  } else if (IsInBRPPool(super_page)) {
    offset = brp_pool_shadow_offset_;
    pool_fd = brp_pool_fd_;
    base_address = BRPPoolBase();
  } else if (IsInConfigurablePool(super_page)) {
    offset = configurable_pool_shadow_offset_;
    pool_fd = configurable_pool_fd_;
    base_address = ConfigurablePoolBase();
  } else {
    PA_NOTREACHED();
  }

  uintptr_t metadata = super_page + SystemPageSize();
  size_t file_offset = (super_page - base_address) >> kSuperPageShift
                                                          << SystemPageShift();

#if PA_BUILDFLAG(IS_POSIX)
  uintptr_t writable_metadata = metadata + offset;
  void* ptr = mmap(reinterpret_cast<void*>(writable_metadata), SystemPageSize(),
                   PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, pool_fd,
                   file_offset);
  PA_CHECK(ptr != MAP_FAILED);
  PA_CHECK(ptr == reinterpret_cast<void*>(writable_metadata));

  if (copy_metadata) [[unlikely]] {
    // Copy the metadata from the private and copy-on-write page to
    // the shared page. (=update the memory file)
    memcpy(reinterpret_cast<void*>(writable_metadata),
           reinterpret_cast<void*>(metadata), SystemPageSize());
  }

  ptr = mmap(reinterpret_cast<void*>(metadata), SystemPageSize(), PROT_READ,
             MAP_FIXED | MAP_SHARED, pool_fd, file_offset);
  PA_CHECK(ptr != MAP_FAILED);
  PA_CHECK(ptr == reinterpret_cast<void*>(metadata));
#else
  // Not implemneted yet.
  PA_NOTREACHED();
#endif  // PA_BUILDFLAG(IS_POSIX)
}

// Regarding normal buckets, metadata will not be decommitted. However,
// regarding direct-mapped, metadata will be decommitted (see UnmapNow()).
// So shadow metadata must be also decommitted (including zero-initialization).
void PartitionAddressSpace::UnmapShadowMetadata(uintptr_t super_page,
                                                pool_handle pool) {
  PA_DCHECK(0u == (super_page & kSuperPageOffsetMask));
  std::ptrdiff_t offset;

  switch (pool) {
    case kRegularPoolHandle:
      PA_DCHECK(RegularPoolBase() <= super_page);
      PA_DCHECK((super_page - RegularPoolBase()) < RegularPoolSize());
      PA_DCHECK(IsShadowMetadataEnabled(kRegularPoolHandle));
      offset = regular_pool_shadow_offset_;
      break;
    case kBRPPoolHandle:
      PA_DCHECK(BRPPoolBase() <= super_page);
      PA_DCHECK((super_page - BRPPoolBase()) < BRPPoolSize());
      PA_DCHECK(IsShadowMetadataEnabled(kBRPPoolHandle));
      offset = brp_pool_shadow_offset_;
      break;
    case kConfigurablePoolHandle:
      PA_DCHECK(IsShadowMetadataEnabled(kConfigurablePoolHandle));
      offset = configurable_pool_shadow_offset_;
      break;
    default:
      return;
  }

  uintptr_t writable_metadata = super_page + SystemPageSize() + offset;

  void* ptr = reinterpret_cast<void*>(writable_metadata);

  // When mapping the page again, we will use mmap() with MAP_FIXED |
  // MAP_SHARED. Not with MAP_ANONYMOUS. If we don't clear the page here, the
  // page will have the same content when re-mapped.
  // TODO(crbug.com/40238514): Make PartitionAlloc not depend on that metadata
  // pages have been already initialized to be zero. i.e. remove memset() below
  // and make the constructors of SlotSpanMetadata, PartitionPageMetadata (and
  // more struct/class if needed) initialize their members. Add test to check
  // if the initialization is correctly done.
  memset(ptr, 0, SystemPageSize());

#if PA_BUILDFLAG(IS_POSIX)
  void* ret = mmap(ptr, SystemPageSize(), PROT_NONE,
                   MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  PA_CHECK(ret != MAP_FAILED);
  PA_CHECK(ret == ptr);
#else
  // Not implemented yet.
  PA_NOTREACHED();
#endif  // PA_BUILDFLAG(IS_POSIX)
}

#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

#if defined(PARTITION_ALLOCATOR_CONSTANTS_POSIX_NONCONST_PAGE_SIZE)

PageCharacteristics page_characteristics;

#endif

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
