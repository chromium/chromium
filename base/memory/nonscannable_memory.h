// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_NONSCANNABLE_MEMORY_H_
#define BASE_MEMORY_NONSCANNABLE_MEMORY_H_

#include <cstdint>

#include <atomic>
#include <memory>

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/base_export.h"
#include "base/no_destructor.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/partition_alloc.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#endif
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// This file contains allocation/deallocation functions for memory that doesn't
// need to be scanned by PCScan. Such memory should only contain "data" objects,
// i.e. objects that don't have pointers/references to other objects. An example
// would be strings or socket/IPC/file buffers. Use with caution.
namespace base {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
namespace internal {

// Represents allocator that contains memory for data-like objects (that don't
// contain pointers) and therefore doesn't require scanning.
template <bool Quarantinable>
class BASE_EXPORT NonScannableAllocatorImpl final {
 public:
  static NonScannableAllocatorImpl& Instance();

  NonScannableAllocatorImpl(const NonScannableAllocatorImpl&) = delete;
  NonScannableAllocatorImpl& operator=(const NonScannableAllocatorImpl&) =
      delete;

  void* Alloc(size_t size);
  static void Free(void*);

  // Returns PartitionRoot corresponding to the allocator, or nullptr if the
  // allocator is not enabled.
  partition_alloc::ThreadSafePartitionRoot* root() {
#if BUILDFLAG(USE_STARSCAN)
    if (!allocator_.get()) {
      return nullptr;
    }
    return allocator_->root();
#else
    return nullptr;
#endif  // BUILDFLAG(USE_STARSCAN)
  }

  void NotifyPCScanEnabled();

 private:
  template <typename>
  friend class base::NoDestructor;

  NonScannableAllocatorImpl();
  ~NonScannableAllocatorImpl();

#if BUILDFLAG(USE_STARSCAN)
  std::unique_ptr<partition_alloc::PartitionAllocator,
                  partition_alloc::internal::PCScanMetadataDeleter>
      allocator_;
  std::atomic_bool pcscan_enabled_{false};
#endif  // BUILDFLAG(USE_STARSCAN)
};

extern template class NonScannableAllocatorImpl<true>;
extern template class NonScannableAllocatorImpl<false>;

using NonScannableAllocator = NonScannableAllocatorImpl<true>;
using NonQuarantinableAllocator = NonScannableAllocatorImpl<false>;

}  // namespace internal
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Allocate/free non-scannable, but still quarantinable memory.
BASE_EXPORT void* AllocNonScannable(size_t size);
BASE_EXPORT void FreeNonScannable(void* ptr);

// Allocate/free non-scannable and non-quarantinable memory. These functions
// behave as normal, *Scan-unaware allocation functions. This can be useful for
// allocations that are guaranteed to be safe by the user, i.e. allocations that
// cannot be referenced from outside and cannot contain dangling references
// themselves.
BASE_EXPORT void* AllocNonQuarantinable(size_t size);
BASE_EXPORT void FreeNonQuarantinable(void* ptr);

// Deleters to be used with std::unique_ptr.
struct NonScannableDeleter {
  void operator()(void* ptr) const { FreeNonScannable(ptr); }
};
struct NonQuarantinableDeleter {
  void operator()(void* ptr) const { FreeNonQuarantinable(ptr); }
};

}  // namespace base

#endif  // BASE_MEMORY_NONSCANNABLE_MEMORY_H_
