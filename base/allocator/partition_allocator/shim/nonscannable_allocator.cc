// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/shim/nonscannable_allocator.h"

#include "base/allocator/partition_allocator/partition_root.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#endif
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace allocator_shim::internal {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
template <bool quarantinable>
NonScannableAllocatorImpl<quarantinable>::NonScannableAllocatorImpl() = default;
template <bool quarantinable>
NonScannableAllocatorImpl<quarantinable>::~NonScannableAllocatorImpl() =
    default;

template <bool quarantinable>
NonScannableAllocatorImpl<quarantinable>&
NonScannableAllocatorImpl<quarantinable>::Instance() {
  static partition_alloc::internal::base::NoDestructor<
      NonScannableAllocatorImpl>
      instance;
  return *instance;
}

template <bool quarantinable>
void* NonScannableAllocatorImpl<quarantinable>::Alloc(size_t size) {
#if BUILDFLAG(USE_STARSCAN)
  // TODO(bikineev): Change to LIKELY once PCScan is enabled by default.
  if (PA_UNLIKELY(pcscan_enabled_.load(std::memory_order_acquire))) {
    PA_DCHECK(allocator_.get());
    return allocator_->root()
        ->AllocInline<partition_alloc::AllocFlags::kNoHooks>(size);
  }
#endif  // BUILDFLAG(USE_STARSCAN)
  // Otherwise, dispatch to default partition.
  return allocator_shim::internal::PartitionAllocMalloc::Allocator()
      ->AllocInline<partition_alloc::AllocFlags::kNoHooks>(size);
}

template <bool quarantinable>
void NonScannableAllocatorImpl<quarantinable>::Free(void* ptr) {
#if BUILDFLAG(USE_STARSCAN)
  if (PA_UNLIKELY(pcscan_enabled_.load(std::memory_order_acquire))) {
    allocator_->root()->FreeInline<partition_alloc::FreeFlags::kNoHooks>(ptr);
    return;
  }
#endif  // BUILDFLAG(USE_STARSCAN)
  partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<
      partition_alloc::FreeFlags::kNoHooks>(ptr);
}

template <bool quarantinable>
void NonScannableAllocatorImpl<quarantinable>::NotifyPCScanEnabled() {
#if BUILDFLAG(USE_STARSCAN)
  allocator_.reset(partition_alloc::internal::MakePCScanMetadata<
                   partition_alloc::PartitionAllocator>(
      partition_alloc::PartitionOptions{
          .star_scan_quarantine =
              quarantinable ? partition_alloc::PartitionOptions::kAllowed
                            : partition_alloc::PartitionOptions::kDisallowed,
          .backup_ref_ptr = partition_alloc::PartitionOptions::kDisabled,
      }));
  if constexpr (quarantinable) {
    partition_alloc::internal::PCScan::RegisterNonScannableRoot(
        allocator_->root());
  }
  pcscan_enabled_.store(true, std::memory_order_release);
#endif  // BUILDFLAG(USE_STARSCAN)
}

template class NonScannableAllocatorImpl<true>;
template class NonScannableAllocatorImpl<false>;

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace allocator_shim::internal
