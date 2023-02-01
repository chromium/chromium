// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/nonscannable_memory.h"

#include <stdlib.h>

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#endif
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base {
namespace internal {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
template <bool Quarantinable>
NonScannableAllocatorImpl<Quarantinable>::NonScannableAllocatorImpl() = default;
template <bool Quarantinable>
NonScannableAllocatorImpl<Quarantinable>::~NonScannableAllocatorImpl() =
    default;

template <bool Quarantinable>
NonScannableAllocatorImpl<Quarantinable>&
NonScannableAllocatorImpl<Quarantinable>::Instance() {
  static base::NoDestructor<NonScannableAllocatorImpl> instance;
  return *instance;
}

template <bool Quarantinable>
void* NonScannableAllocatorImpl<Quarantinable>::Alloc(size_t size) {
#if BUILDFLAG(USE_STARSCAN)
  // TODO(bikineev): Change to LIKELY once PCScan is enabled by default.
  if (UNLIKELY(pcscan_enabled_.load(std::memory_order_acquire))) {
    PA_DCHECK(allocator_.get());
    return allocator_->root()->AllocWithFlagsNoHooks(
        0, size, partition_alloc::PartitionPageSize());
  }
#endif  // BUILDFLAG(USE_STARSCAN)
  // Otherwise, dispatch to default partition.
  return allocator_shim::internal::PartitionAllocMalloc::Allocator()
      ->AllocWithFlagsNoHooks(0, size, partition_alloc::PartitionPageSize());
}

template <bool Quarantinable>
void NonScannableAllocatorImpl<Quarantinable>::Free(void* ptr) {
  partition_alloc::ThreadSafePartitionRoot::FreeNoHooks(ptr);
}

template <bool Quarantinable>
void NonScannableAllocatorImpl<Quarantinable>::NotifyPCScanEnabled() {
#if BUILDFLAG(USE_STARSCAN)
  allocator_.reset(partition_alloc::internal::MakePCScanMetadata<
                   partition_alloc::PartitionAllocator>());
  allocator_->init({
      partition_alloc::PartitionOptions::AlignedAlloc::kDisallowed,
      partition_alloc::PartitionOptions::ThreadCache::kDisabled,
      Quarantinable
          ? partition_alloc::PartitionOptions::Quarantine::kAllowed
          : partition_alloc::PartitionOptions::Quarantine::kDisallowed,
      partition_alloc::PartitionOptions::Cookie::kAllowed,
      partition_alloc::PartitionOptions::BackupRefPtr::kDisabled,
      partition_alloc::PartitionOptions::BackupRefPtrZapping::kDisabled,
      partition_alloc::PartitionOptions::UseConfigurablePool::kNo,
  });
  if (Quarantinable) {
    partition_alloc::internal::PCScan::RegisterNonScannableRoot(
        allocator_->root());
  }
  pcscan_enabled_.store(true, std::memory_order_release);
#endif  // BUILDFLAG(USE_STARSCAN)
}

template class NonScannableAllocatorImpl<true>;
template class NonScannableAllocatorImpl<false>;

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace internal

void* AllocNonScannable(size_t size) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return internal::NonScannableAllocatorImpl</*Quarantinable=*/true>::Instance()
      .Alloc(size);
#else
  return ::malloc(size);
#endif
}

void FreeNonScannable(void* ptr) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  internal::NonScannableAllocatorImpl</*Quarantinable=*/true>::Free(ptr);
#else
  return ::free(ptr);
#endif
}

void* AllocNonQuarantinable(size_t size) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return internal::NonScannableAllocatorImpl<
             /*Quarantinable=*/false>::Instance()
      .Alloc(size);
#else
  return ::malloc(size);
#endif
}

void FreeNonQuarantinable(void* ptr) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  internal::NonScannableAllocatorImpl</*Quarantinable=*/false>::Free(ptr);
#else
  return ::free(ptr);
#endif
}

}  // namespace base
