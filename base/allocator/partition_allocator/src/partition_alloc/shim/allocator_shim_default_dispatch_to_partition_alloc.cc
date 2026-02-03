// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

#include <atomic>
#include <cstddef>
#include <map>
#include <string>
#include <tuple>

#include "partition_alloc/allocation_guard.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/memory_reclaimer.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/export_template.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_base/numerics/checked_math.h"
#include "partition_alloc/partition_alloc_base/numerics/safe_conversions.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"
#include "partition_alloc/shim/allocator_dispatch.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_internal.h"
#include "partition_alloc/shim/allocator_shim_internals.h"

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#endif

using allocator_shim::AllocatorDispatch;

namespace allocator_shim {

namespace {

class SimpleScopedSpinLocker {
 public:
  explicit SimpleScopedSpinLocker(std::atomic<bool>& lock) : lock_(lock) {
    // Lock. Semantically equivalent to base::Lock::Acquire().
    bool expected = false;
    // Weak CAS since we are in a retry loop, relaxed ordering for failure since
    // in this case we don't imply any ordering.
    //
    // This matches partition_allocator/spinning_mutex.h fast path on Linux.
    while (!lock_.compare_exchange_weak(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = false;
    }
  }

  ~SimpleScopedSpinLocker() { lock_.store(false, std::memory_order_release); }

 private:
  std::atomic<bool>& lock_;
};

// We can't use a "static local" or a base::LazyInstance, as:
// - static local variables call into the runtime on Windows, which is not
//   prepared to handle it, as the first allocation happens during CRT init.
// - We don't want to depend on base::LazyInstance, which may be converted to
//   static locals one day.
//
// Nevertheless, this provides essentially the same thing.
template <typename T, typename Constructor>
class LeakySingleton {
 public:
  constexpr LeakySingleton() = default;

  PA_ALWAYS_INLINE T* Get() {
    auto* instance = instance_.load(std::memory_order_acquire);
    if (instance) [[likely]] {
      return instance;
    }

    return GetSlowPath();
  }

  // Replaces the instance pointer with a new one.
  void Replace(T* new_instance) {
    SimpleScopedSpinLocker scoped_lock{initialization_lock_};

    // Modify under the lock to avoid race between |if (instance)| and
    // |instance_.store()| in GetSlowPath().
    instance_.store(new_instance, std::memory_order_release);
  }

 private:
  T* GetSlowPath();

  std::atomic<T*> instance_;
  // Before C++20, having an initializer here causes a "variable does not have a
  // constant initializer" error.  In C++20, omitting it causes a similar error.
  // Presumably this is due to the C++20 changes to make atomic initialization
  // (of the other members of this class) sane, so guarding under that
  // feature-test.
#if !defined(__cpp_lib_atomic_value_initialization) || \
    __cpp_lib_atomic_value_initialization < 201911L
  alignas(T) uint8_t instance_buffer_[sizeof(T)];
#else
  alignas(T) uint8_t instance_buffer_[sizeof(T)] = {};
#endif
  std::atomic<bool> initialization_lock_;
};

template <typename T, typename Constructor>
T* LeakySingleton<T, Constructor>::GetSlowPath() {
  // The instance has not been set, the proper way to proceed (correct
  // double-checked locking) is:
  //
  // auto* instance = instance_.load(std::memory_order_acquire);
  // if (!instance) {
  //   ScopedLock initialization_lock;
  //   root = instance_.load(std::memory_order_relaxed);
  //   if (root)
  //     return root;
  //   instance = Create new root;
  //   instance_.store(instance, std::memory_order_release);
  //   return instance;
  // }
  //
  // However, we don't want to use a base::Lock here, so instead we use
  // compare-and-exchange on a lock variable, which provides the same
  // guarantees.
  SimpleScopedSpinLocker scoped_lock{initialization_lock_};

  T* instance = instance_.load(std::memory_order_relaxed);
  // Someone beat us.
  if (instance) {
    return instance;
  }

  instance = Constructor::New(reinterpret_cast<void*>(instance_buffer_));
  instance_.store(instance, std::memory_order_release);

  return instance;
}

class MainPartitionConstructor {
 public:
  static partition_alloc::PartitionRoot* New(void* buffer) {
    partition_alloc::PartitionOptions opts;
    // Only one partition can have thread cache enabled. Since, additional
    // partitions are created in ReconfigureAfterFeatureListInit(), postpone
    // the decision to turn the thread cache on until then.
    // Also tests, such as the ThreadCache tests create a thread cache.
    opts.thread_cache = partition_alloc::PartitionOptions::kDisabled;
    opts.backup_ref_ptr = partition_alloc::PartitionOptions::kDisabled;
    auto* new_root = new (buffer) partition_alloc::PartitionRoot(opts);

    return new_root;
  }
};

LeakySingleton<partition_alloc::PartitionRoot, MainPartitionConstructor>
    g_roots[kMaxAllocToken.value() + 1] = {};

partition_alloc::PartitionRoot* Allocator(AllocToken alloc_token) {
  PA_DCHECK(alloc_token <= kMaxAllocToken);
#if PA_BUILDFLAG(SHIM_SUPPORTS_ALLOC_TOKEN)
  return PA_UNSAFE_TODO(g_roots[alloc_token.value()]).Get();
#else
  return g_roots[0].Get();
#endif
}

// Original g_root_ if it was replaced by ConfigurePartitions().
std::atomic<partition_alloc::PartitionRoot*>
    g_original_roots[kMaxAllocToken.value() + 1] = {};

std::atomic<bool> g_roots_finalized = false;

partition_alloc::PartitionRoot* OriginalAllocator(AllocToken alloc_token) {
  return PA_UNSAFE_TODO(g_original_roots[alloc_token.value()])
      .load(std::memory_order_relaxed);
}

bool AllocatorConfigurationFinalized() {
  return g_roots_finalized.load();
}

template <partition_alloc::AllocFlags flags>
void* AllocateAlignedMemory(size_t alignment,
                            size_t size,
                            AllocToken alloc_token) {
  // Memory returned by the regular allocator *always* respects |kAlignment|,
  // which is a power of two, and any valid alignment is also a power of two. So
  // we can directly fulfill these requests with the regular Alloc function.
  //
  // There are several call sites in Chromium where base::AlignedAlloc is called
  // with a small alignment. Some may be due to overly-careful code, some are
  // because the client code doesn't know the required alignment at compile
  // time.
  if (alignment <= partition_alloc::internal::kAlignment) {
    // This is mandated by |posix_memalign()| and friends, so should never fire.
    PA_CHECK(partition_alloc::internal::base::bits::HasSingleBit(alignment));
    // TODO(bartekn): See if the compiler optimizes branches down the stack on
    // Mac, where PartitionPageSize() isn't constexpr.
    return Allocator(alloc_token)->AllocInline<flags>(size);
  }

  return Allocator(alloc_token)->AlignedAllocInline<flags>(alignment, size);
}

}  // namespace

namespace internal {

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    Malloc(size_t size, AllocToken alloc_token, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return Allocator(alloc_token)->AllocInline<base_alloc_flags>(size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    MallocUnchecked(size_t size, AllocToken alloc_token, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return Allocator(alloc_token)
      ->AllocInline<base_alloc_flags |
                    partition_alloc::AllocFlags::kReturnNull>(size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    Calloc(size_t n, size_t size, AllocToken alloc_token, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  const size_t total =
      partition_alloc::internal::base::CheckMul(n, size).ValueOrDie();
  return Allocator(alloc_token)
      ->AllocInline<base_alloc_flags | partition_alloc::AllocFlags::kZeroFill>(
          total);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    CallocUnchecked(size_t n,
                    size_t size,
                    AllocToken alloc_token,
                    void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  const size_t total =
      partition_alloc::internal::base::CheckMul(n, size).ValueOrDie();
  return Allocator(alloc_token)
      ->AllocInline<base_alloc_flags |
                    partition_alloc::AllocFlags::kReturnNull |
                    partition_alloc::AllocFlags::kZeroFill>(total);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    Memalign(size_t alignment,
             size_t size,
             AllocToken alloc_token,
             void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory<base_alloc_flags>(alignment, size, alloc_token);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    AlignedAlloc(size_t size,
                 size_t alignment,
                 AllocToken alloc_token,
                 void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory<base_alloc_flags>(alignment, size, alloc_token);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    AlignedAllocUnchecked(size_t size,
                          size_t alignment,
                          AllocToken alloc_token,
                          void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory<base_alloc_flags |
                               partition_alloc::AllocFlags::kReturnNull>(
      alignment, size, alloc_token);
}

// aligned_realloc documentation is
// https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/reference/aligned-realloc
// TODO(tasak): Expand the given memory block to the given size if possible.
// This realloc always free the original memory block and allocates a new memory
// block.
// TODO(tasak): Implement PartitionRoot::AlignedRealloc and use it.
// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    AlignedRealloc(void* address,
                   size_t size,
                   size_t alignment,
                   AllocToken alloc_token,
                   void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  void* new_ptr = nullptr;
  if (size > 0) {
    new_ptr =
        AllocateAlignedMemory<base_alloc_flags>(alignment, size, alloc_token);
  } else {
    // size == 0 and address != null means just "free(address)".
    if (address) {
      partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
          address);
    }
  }
  // The original memory block (specified by address) is unchanged if ENOMEM.
  if (!new_ptr) {
    return nullptr;
  }
  // TODO(tasak): Need to compare the new alignment with the address' alignment.
  // If the two alignments are not the same, need to return nullptr with EINVAL.
  if (address) {
    size_t usage = partition_alloc::PartitionRoot::GetUsableSize(address);
    size_t copy_size = usage > size ? size : usage;
    PA_UNSAFE_TODO(memcpy(new_ptr, address, copy_size));

    partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
        address);
  }
  return new_ptr;
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    AlignedReallocUnchecked(void* address,
                            size_t size,
                            size_t alignment,
                            AllocToken alloc_token,
                            void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  void* new_ptr = nullptr;
  if (size > 0) {
    new_ptr = AllocateAlignedMemory<base_alloc_flags |
                                    partition_alloc::AllocFlags::kReturnNull>(
        alignment, size, alloc_token);
  } else {
    // size == 0 and address != null means just "free(address)".
    if (address) {
      partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
          address);
    }
  }
  // The original memory block (specified by address) is unchanged if ENOMEM.
  if (!new_ptr) {
    return nullptr;
  }
  // TODO(tasak): Need to compare the new alignment with the address' alignment.
  // If the two alignments are not the same, need to return nullptr with EINVAL.
  if (address) {
    size_t usage = partition_alloc::PartitionRoot::GetUsableSize(address);
    size_t copy_size = usage > size ? size : usage;
    PA_UNSAFE_TODO(memcpy(new_ptr, address, copy_size));

    partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
        address);
  }
  return new_ptr;
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    Realloc(void* address, size_t size, AllocToken alloc_token, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
#if PA_BUILDFLAG(IS_APPLE)
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(address)) &&
      address) [[unlikely]] {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `realloc` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return realloc(address, size);
  }
#endif  // PA_BUILDFLAG(IS_APPLE)

  // PartitionRoot::Realloc uses the root only when the address is nullptr;
  // otherwise it uses the root calculated from the address.　Therefore,
  // Allocator(alloc_token) is safe even if the token is different from the one
  // used in malloc.
  return Allocator(alloc_token)
      ->Realloc<base_alloc_flags, base_free_flags>(address, size, "");
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    ReallocUnchecked(void* address,
                     size_t size,
                     AllocToken alloc_token,
                     void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
#if PA_BUILDFLAG(IS_APPLE)
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(address)) &&
      address) [[unlikely]] {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `realloc` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return realloc(address, size);
  }
#endif  // PA_BUILDFLAG(IS_APPLE)

  return Allocator(alloc_token)
      ->Realloc<base_alloc_flags | partition_alloc::AllocFlags::kReturnNull>(
          address, size, "");
}

#if PA_BUILDFLAG(IS_CAST_ANDROID)
extern "C" {
void __real_free(void*);
}  // extern "C"
#endif  // PA_BUILDFLAG(IS_CAST_ANDROID)

constexpr bool MightNeedToHandleSystemDeallocation() {
#if PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(IS_CAST_ANDROID)
  return true;
#else
  return false;
#endif
}

PA_ALWAYS_INLINE bool MaybeHandleSystemDeallocation(void* object) {
#if PA_BUILDFLAG(IS_APPLE)
  // TODO(bartekn): Add MTE unmasking here (and below).
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(object)) &&
      object) [[unlikely]] {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    free(object);
    return true;
  }
#endif  // PA_BUILDFLAG(IS_APPLE)

  // On Android Chromecast devices, there is at least one case where a system
  // malloc() pointer can be passed to PartitionAlloc's free(). If we don't own
  // the pointer, pass it along. This should not have a runtime cost vs regular
  // Android, since on Android we have a PA_CHECK() rather than the branch here.
#if PA_BUILDFLAG(IS_CAST_ANDROID)
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(object)) &&
      object) [[unlikely]] {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free()`, which is `__real_free()`
    // here.
    __real_free(object);
    return true;
  }
#endif  // PA_BUILDFLAG(IS_CAST_ANDROID)
  return false;
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
PA_ALWAYS_INLINE void
PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::Free(
    void* object,
    void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  // We create separate constexpr branch just to optimize this path on platforms
  // where we don't need to check MaybeHandleSystemDeallocation.
  if constexpr (MightNeedToHandleSystemDeallocation()) {
    if (MaybeHandleSystemDeallocation(object)) [[unlikely]] {
      return;
    }
  }
  partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
      object);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
PA_ALWAYS_INLINE void
PartitionAllocFunctionsInternal<base_alloc_flags,
                                base_free_flags>::FreeWithSize(void* object,
                                                               size_t size,
                                                               void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  // We create separate constexpr branch just to optimize this path on platforms
  // where we don't need to check MaybeHandleSystemDeallocation.
  if constexpr (MightNeedToHandleSystemDeallocation()) {
    if (MaybeHandleSystemDeallocation(object)) [[unlikely]] {
      return;
    }
  }
  partition_alloc::PartitionRoot::FreeWithSizeInlineInUnknownRoot<
      base_free_flags>(object, size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
PA_ALWAYS_INLINE void
PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    FreeWithAlignment(void* object, size_t alignment, void* context) {
  // TODO(lizeb): Optimize PartitionAlloc to use the size information. This is
  // still useful though, as we avoid double-checking that the address is owned.
  PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::Free(
      object, context);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
PA_ALWAYS_INLINE void PartitionAllocFunctionsInternal<
    base_alloc_flags,
    base_free_flags>::FreeWithSizeAndAlignment(void* object,
                                               size_t size,
                                               size_t alignment,
                                               void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  // We create separate constexpr branch just to optimize this path on platforms
  // where we don't need to check MaybeHandleSystemDeallocation.
  if constexpr (MightNeedToHandleSystemDeallocation()) {
    if (MaybeHandleSystemDeallocation(object)) [[unlikely]] {
      return;
    }
  }
  // While `AllocateAlignedMemory` uses a standard `Alloc` for small alignments
  // to improve speed and reduce memory fragmentation, we always use aligned
  // Free here. This is because: 1) `GetAdjustedSizeForAlignment` handles small
  // alignments, ensuring correct size adjustments, 2) Alignment only affects
  // the size determination, so always calling aligned Free doesn't incur
  // overhead, and 3) it avoids the binary size increase.
  partition_alloc::PartitionRoot::FreeWithSizeAndAlignmentInlineInUnknownRoot<
      base_free_flags>(object, size, alignment);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
size_t PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    GetSizeEstimate(void* address, void* context) {
  // This is used to implement malloc_usable_size(3). Per its man page, "if ptr
  // is NULL, 0 is returned".
  if (!address) {
    return 0;
  }

#if PA_BUILDFLAG(IS_APPLE)
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(address))) {
    // The object pointed to by `address` is not allocated by the
    // PartitionAlloc.  The return value `0` means that the pointer does not
    // belong to this malloc zone.
    return 0;
  }
#endif  // PA_BUILDFLAG(IS_APPLE)

  // TODO(lizeb): Returns incorrect values for aligned allocations.
  const size_t size = partition_alloc::PartitionRoot::GetUsableSize(address);
#if PA_BUILDFLAG(IS_APPLE)
  // The object pointed to by `address` is allocated by the PartitionAlloc.
  // So, this function must not return zero so that the malloc zone dispatcher
  // finds the appropriate malloc zone.
  PA_DCHECK(size);
#endif  // PA_BUILDFLAG(IS_APPLE)
  return size;
}

#if PA_BUILDFLAG(IS_APPLE)
// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
size_t
PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::GoodSize(
    size_t size,
    void* context) {
  return Allocator(kDefaultAllocToken)
      ->AllocationCapacityFromRequestedSize(size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
bool PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    ClaimedAddress(void* address, void* context) {
  return partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(address));
}
#endif  // PA_BUILDFLAG(IS_APPLE)

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
unsigned
PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::BatchMalloc(
    size_t size,
    void** results,
    unsigned num_requested,
    void* context) {
  // No real batching: we could only acquire the lock once for instance, keep it
  // simple for now.
  for (unsigned i = 0; i < num_requested; i++) {
    // No need to check the results, we crash if it fails.
    PA_UNSAFE_TODO(results[i]) = Malloc(size, kDefaultAllocToken, nullptr);
  }

  // Either all succeeded, or we crashed.
  return num_requested;
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    BatchFree(void** to_be_freed, unsigned num_to_be_freed, void* context) {
  // No real batching: we could only acquire the lock once for instance, keep it
  // simple for now.
  for (unsigned i = 0; i < num_to_be_freed; i++) {
    Free(PA_UNSAFE_TODO(to_be_freed[i]), nullptr);
  }
}

#if PA_BUILDFLAG(IS_APPLE)
// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    TryFreeDefault(void* address, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};

  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(address))) [[unlikely]] {
    // The object pointed to by `address` is not allocated by the
    // PartitionAlloc. Call find_zone_and_free.
    return allocator_shim::TryFreeDefaultFallbackToFindZoneAndFree(address);
  }

  partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
      address);
}
#endif  // PA_BUILDFLAG(IS_APPLE)

// Explicitly instantiate `PartitionAllocFunctions`.
template class PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(ALLOCATOR_SHIM))
    PartitionAllocFunctionsInternal<partition_alloc::AllocFlags::kNoHooks,
                                    partition_alloc::FreeFlags::kNoHooks>;
// Explicitly instantiate `PartitionAllocWithAdvancedChecksFunctions`.
template class PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(ALLOCATOR_SHIM))
    PartitionAllocFunctionsInternal<
        partition_alloc::AllocFlags::kNoHooks,
        partition_alloc::FreeFlags::kNoHooks |
            partition_alloc::FreeFlags::kSchedulerLoopQuarantine>;

// static
bool PartitionAllocMalloc::AllocatorConfigurationFinalized() {
  return ::allocator_shim::AllocatorConfigurationFinalized();
}

// static
partition_alloc::PartitionRoot* PartitionAllocMalloc::Allocator(
    AllocToken alloc_token) {
  return ::allocator_shim::Allocator(alloc_token);
}

// static
partition_alloc::PartitionRoot* PartitionAllocMalloc::OriginalAllocator(
    AllocToken alloc_token) {
  return ::allocator_shim::OriginalAllocator(alloc_token);
}

}  // namespace internal

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

std::atomic<const AllocatorDispatch*> g_delegate_dispatch =
    &internal::kPartitionAllocDispatch;

PA_ALWAYS_INLINE const AllocatorDispatch* GetDelegate() {
  return g_delegate_dispatch.load(std::memory_order_relaxed);
}

void* DelegatedAllocFn(size_t size, AllocToken alloc_token, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_function(size, alloc_token, context);
}

void* DelegatedAllocUncheckedFn(size_t size,
                                AllocToken alloc_token,
                                void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_unchecked_function(size, alloc_token,
                                                        context);
}

void* DelegatedAllocZeroInitializedFn(size_t n,
                                      size_t size,
                                      AllocToken alloc_token,
                                      void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_zero_initialized_function(
      n, size, alloc_token, context);
}

void* DelegatedAllocZeroInitializedUncheckedFn(size_t n,
                                               size_t size,
                                               AllocToken alloc_token,
                                               void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_zero_initialized_unchecked_function(
      n, size, alloc_token, context);
}

void* DelegatedAllocAlignedFn(size_t alignment,
                              size_t size,
                              AllocToken alloc_token,
                              void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_aligned_function(alignment, size,
                                                      alloc_token, context);
}

void* DelegatedReallocFn(void* address,
                         size_t size,
                         AllocToken alloc_token,
                         void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->realloc_function(address, size, alloc_token,
                                                context);
}

void* DelegatedReallocUncheckedFn(void* address,
                                  size_t size,
                                  AllocToken alloc_token,
                                  void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->realloc_unchecked_function(address, size,
                                                          alloc_token, context);
}

void DelegatedFreeFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_function(address, context);
}

void DelegatedFreeWithSizeFn(void* address, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_with_size_function(address, size, context);
}

void DelegatedFreeWithAlignmentFn(void* address,
                                  size_t alignment,
                                  void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_with_alignment_function(address, alignment,
                                                            context);
}

void DelegatedFreeWithSizeAndAlignmentFn(void* address,
                                         size_t size,
                                         size_t alignment,
                                         void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_with_size_and_alignment_function(
      address, size, alignment, context);
}

size_t DelegatedGetSizeEstimateFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->get_size_estimate_function(address, context);
}

size_t DelegatedGoodSizeFn(size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->good_size_function(size, context);
}

bool DelegatedClaimedAddressFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->claimed_address_function(address, context);
}

unsigned DelegatedBatchMallocFn(size_t size,
                                void** results,
                                unsigned num_requested,
                                void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->batch_malloc_function(size, results,
                                                     num_requested, context);
}

void DelegatedBatchFreeFn(void** to_be_freed,
                          unsigned num_to_be_freed,
                          void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->batch_free_function(to_be_freed, num_to_be_freed,
                                                   context);
}

void DelegatedTryFreeDefaultFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->try_free_default_function(address, context);
}

void* DelegatedAlignedMallocFn(size_t size,
                               size_t alignment,
                               AllocToken alloc_token,
                               void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_malloc_function(size, alignment,
                                                       alloc_token, context);
}

void* DelegatedAlignedMallocUncheckedFn(size_t size,
                                        size_t alignment,
                                        AllocToken alloc_token,
                                        void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_malloc_unchecked_function(
      size, alignment, alloc_token, context);
}

void* DelegatedAlignedReallocFn(void* address,
                                size_t size,
                                size_t alignment,
                                AllocToken alloc_token,
                                void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_realloc_function(
      address, size, alignment, alloc_token, context);
}

void* DelegatedAlignedReallocUncheckedFn(void* address,
                                         size_t size,
                                         size_t alignment,
                                         AllocToken alloc_token,
                                         void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_realloc_unchecked_function(
      address, size, alignment, alloc_token, context);
}

void DelegatedAlignedFreeFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_free_function(address, context);
}

void InstallCustomDispatch(AllocatorDispatch* dispatch) {
  PA_DCHECK(dispatch);

  // Must have followings:
  PA_DCHECK(dispatch->alloc_function != nullptr);
  PA_DCHECK(dispatch->alloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->alloc_zero_initialized_function != nullptr);
  PA_DCHECK(dispatch->alloc_aligned_function != nullptr);
  PA_DCHECK(dispatch->realloc_function != nullptr);
  PA_DCHECK(dispatch->realloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->free_function != nullptr);
  PA_DCHECK(dispatch->get_size_estimate_function != nullptr);
#if PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->good_size_function != nullptr);
  PA_DCHECK(dispatch->claimed_address_function != nullptr);
#endif  // PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->batch_malloc_function != nullptr);
  PA_DCHECK(dispatch->batch_free_function != nullptr);
#if PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->free_with_size_function != nullptr);
  PA_DCHECK(dispatch->try_free_default_function != nullptr);
#endif  // PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->aligned_malloc_function != nullptr);
  PA_DCHECK(dispatch->aligned_malloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->aligned_realloc_function != nullptr);
  PA_DCHECK(dispatch->aligned_realloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->aligned_free_function != nullptr);

  dispatch->next = &internal::kPartitionAllocDispatch;

  // Unlike `InsertAllocatorDispatch(...)`, we don't have any invariant here.
  // Hence using relaxed memory ordering.
#if !PA_BUILDFLAG(DCHECKS_ARE_ON)
  g_delegate_dispatch.store(dispatch, std::memory_order_relaxed);
#else
  const AllocatorDispatch* previous_value =
      g_delegate_dispatch.exchange(dispatch, std::memory_order_relaxed);
  // We also allow `previous_value == dispatch` i.e. `dispatch` is written
  // twice - sometimes it is hard to guarantee "exactly once" initialization.
  PA_DCHECK(previous_value == &internal::kPartitionAllocDispatch ||
            previous_value == dispatch);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
}

}  // namespace

void InstallPartitionAllocWithAdvancedChecks() {
  PA_CONSTINIT static AllocatorDispatch dispatch = []() constexpr {
    auto dispatch =
        internal::PartitionAllocWithAdvancedChecksFunctions::MakeDispatch();
    dispatch.next = &internal::kPartitionAllocDispatch;
    return dispatch;
  }();
  InstallCustomDispatch(&dispatch);
}

void InstallCustomDispatchForTesting(AllocatorDispatch* dispatch) {
  InstallCustomDispatch(dispatch);
}

void UninstallCustomDispatch() {
  g_delegate_dispatch.store(&internal::kPartitionAllocDispatch,
                            std::memory_order_relaxed);
}

void EnablePartitionAllocMemoryReclaimer() {
  for (size_t alloc_token = 0; alloc_token <= kMaxAllocToken.value();
       alloc_token++) {
    // Unlike other partitions, Allocator() does not register its PartitionRoot
    // to the memory reclaimer, because doing so may allocate memory. Thus, the
    // registration to the memory reclaimer has to be done some time later, when
    // the main root is fully configured.
    ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
        Allocator(AllocToken(alloc_token)));

    // There is only one PartitionAlloc-Everywhere partition at the moment. Any
    // additional partitions will be created in ConfigurePartitions() and
    // registered for memory reclaimer there.
    PA_DCHECK(!AllocatorConfigurationFinalized());
    PA_DCHECK(OriginalAllocator(AllocToken(alloc_token)) == nullptr);
  }
}

void ConfigurePartitions(
    EnableBrp enable_brp,
    size_t brp_extra_extras_size,
    EnableMemoryTagging enable_memory_tagging,
    partition_alloc::TagViolationReportingMode memory_tagging_reporting_mode,
    BucketDistribution distribution,
    partition_alloc::internal::SchedulerLoopQuarantineConfig
        scheduler_loop_quarantine_global_config,
    partition_alloc::internal::SchedulerLoopQuarantineConfig
        scheduler_loop_quarantine_thread_local_config,
    partition_alloc::internal::SchedulerLoopQuarantineConfig
        scheduler_loop_quarantine_for_advanced_memory_safety_checks_config,
    EventuallyZeroFreedMemory eventually_zero_freed_memory,
    EnableFreeWithSize enable_free_with_size,
    EnableStrictFreeSizeCheck enable_strict_free_size_check) {
  partition_alloc::PartitionOptions opts;
  // The caller of ConfigurePartitions() will decide whether this or
  // another partition will have the thread cache enabled, by calling
  // EnableThreadCacheIfSupported().

  opts.thread_cache = partition_alloc::PartitionOptions::kDisabled;
  opts.backup_ref_ptr = enable_brp
                            ? partition_alloc::PartitionOptions::kEnabled
                            : partition_alloc::PartitionOptions::kDisabled;
  opts.backup_ref_ptr_extra_extras_size = brp_extra_extras_size;
  opts.eventually_zero_freed_memory =
      eventually_zero_freed_memory
          ? partition_alloc::PartitionOptions::kEnabled
          : partition_alloc::PartitionOptions::kDisabled;
  opts.scheduler_loop_quarantine_global_config =
      scheduler_loop_quarantine_global_config;
  opts.scheduler_loop_quarantine_thread_local_config =
      scheduler_loop_quarantine_thread_local_config;
  opts.scheduler_loop_quarantine_for_advanced_memory_safety_checks_config =
      scheduler_loop_quarantine_for_advanced_memory_safety_checks_config;
  opts.memory_tagging = {
      .enabled = enable_memory_tagging
                     ? partition_alloc::PartitionOptions::kEnabled
                     : partition_alloc::PartitionOptions::kDisabled,
      .reporting_mode = memory_tagging_reporting_mode};
  opts.free_with_size = enable_free_with_size
                            ? partition_alloc::PartitionOptions::kEnabled
                            : partition_alloc::PartitionOptions::kDisabled;
  opts.strict_free_size_check =
      enable_strict_free_size_check
          ? partition_alloc::PartitionOptions::kEnabled
          : partition_alloc::PartitionOptions::kDisabled;

  static partition_alloc::internal::base::NoDestructor<
      partition_alloc::PartitionAllocator>
      new_main_allocators[2] = {
          partition_alloc::internal::base::NoDestructor<
              partition_alloc::PartitionAllocator>([&opts] {
            opts.thread_cache_index = 0;
            return opts;
          }()),
          partition_alloc::internal::base::NoDestructor<
              partition_alloc::PartitionAllocator>([&opts] {
            opts.thread_cache_index = 1;
            return opts;
          }())};

  for (size_t alloc_token = 0; alloc_token <= kMaxAllocToken.value();
       alloc_token++) {
    // Calling Get() is actually important, even if the return value isn't
    // used, because it has a side effect of initializing the variable, if it
    // wasn't already.
    auto* current_root = PA_UNSAFE_TODO(g_roots[alloc_token]).Get();

    // We've been bitten before by using a static local when initializing a
    // partition. For synchronization, static local variables call into the
    // runtime on Windows, which may not be ready to handle it, if the path is
    // invoked on an allocation during the runtime initialization.
    // ConfigurePartitions() is invoked explicitly from Chromium code, so this
    // shouldn't bite us here. Mentioning just in case we move this code
    // earlier.
    partition_alloc::PartitionRoot* new_root =
        PA_UNSAFE_TODO(new_main_allocators[alloc_token])->root();

    // Ensure that we switch `new_root` before directing new traffic to it, this
    // ensures that a BucketDistribution is consistent over the life of an
    // allocation.
    switch (distribution) {
      case BucketDistribution::kNeutral:
        // We start in the 'default' case.
        break;
      case BucketDistribution::kDenser:
        new_root->SwitchToDenserBucketDistribution();
        break;
    }

    // Now switch traffic to the new partition.
    PA_UNSAFE_TODO(g_original_roots[alloc_token]) = current_root;
    PA_UNSAFE_TODO(g_roots[alloc_token]).Replace(new_root);

    // Purge memory, now that the traffic to the original partition is cut off.
    current_root->PurgeMemory(
        partition_alloc::PurgeFlags::kDecommitEmptySlotSpans |
        partition_alloc::PurgeFlags::kDiscardUnusedSystemPages);
  }
  PA_CHECK(!g_roots_finalized.exchange(true));  // Ensure configured once.
}

// No synchronization provided: `PartitionRoot.flags` is only written
// to in `PartitionRoot::Init()`.
uint32_t GetMainPartitionRootExtrasSize() {
#if PA_CONFIG(EXTRAS_REQUIRED)
  return PA_UNSAFE_TODO(g_roots[0]).Get()->settings_.extras_size;
#else
  return 0;
#endif  // PA_CONFIG(EXTRAS_REQUIRED)
}

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    .alloc_function = &DelegatedAllocFn,
    .alloc_unchecked_function = &DelegatedAllocUncheckedFn,
    .alloc_zero_initialized_function = &DelegatedAllocZeroInitializedFn,
    .alloc_zero_initialized_unchecked_function =
        &DelegatedAllocZeroInitializedUncheckedFn,
    .alloc_aligned_function = &DelegatedAllocAlignedFn,
    .realloc_function = &DelegatedReallocFn,
    .realloc_unchecked_function = &DelegatedReallocUncheckedFn,
    .free_function = &DelegatedFreeFn,
    .free_with_size_function = &DelegatedFreeWithSizeFn,
    .free_with_alignment_function = &DelegatedFreeWithAlignmentFn,
    .free_with_size_and_alignment_function =
        &DelegatedFreeWithSizeAndAlignmentFn,
    .get_size_estimate_function = &DelegatedGetSizeEstimateFn,
    .good_size_function = &DelegatedGoodSizeFn,
    .claimed_address_function = &DelegatedClaimedAddressFn,
    .batch_malloc_function = &DelegatedBatchMallocFn,
    .batch_free_function = &DelegatedBatchFreeFn,
    .try_free_default_function = &DelegatedTryFreeDefaultFn,
    .aligned_malloc_function = &DelegatedAlignedMallocFn,
    .aligned_malloc_unchecked_function = &DelegatedAlignedMallocUncheckedFn,
    .aligned_realloc_function = &DelegatedAlignedReallocFn,
    .aligned_realloc_unchecked_function = &DelegatedAlignedReallocUncheckedFn,
    .aligned_free_function = &DelegatedAlignedFreeFn,
    .next = nullptr,
};

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace allocator_shim

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

extern "C" {

#if !PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(IS_ANDROID)

SHIM_ALWAYS_EXPORT void malloc_stats(void) __THROW {}

SHIM_ALWAYS_EXPORT int mallopt(int cmd, int value) __THROW {
  return 0;
}

#endif  // !PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(IS_ANDROID)

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
  partition_alloc::SimplePartitionStatsDumper allocator_dumper;
  // TODO(crbug.com/477186304): Dump stats for all alloc tokens, by accumulating
  // the stats or separating reporting stats.
  allocator_shim::Allocator(kDefaultAllocToken)
      ->DumpStats("malloc", true, &allocator_dumper);

  struct mallinfo info = {};
  info.arena = 0;  // Memory *not* allocated with mmap().

  // Memory allocated with mmap(), aka virtual size.
  info.hblks =
      partition_alloc::internal::base::checked_cast<decltype(info.hblks)>(
          allocator_dumper.stats().total_mmapped_bytes);
  // Resident bytes.
  info.hblkhd =
      partition_alloc::internal::base::checked_cast<decltype(info.hblkhd)>(
          allocator_dumper.stats().total_resident_bytes);
  // Allocated bytes.
  info.uordblks =
      partition_alloc::internal::base::checked_cast<decltype(info.uordblks)>(
          allocator_dumper.stats().total_active_bytes);

  return info;
}
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

}  // extern "C"

#if PA_BUILDFLAG(IS_APPLE)

namespace allocator_shim {

void InitializeDefaultAllocatorPartitionRoot() {
  // On OS_APPLE, the initialization of PartitionRoot uses memory allocations
  // internally, e.g. __builtin_available, and it's not easy to avoid it.
  // Thus, we initialize the PartitionRoot with using the system default
  // allocator before we intercept the system default allocator.
  for (size_t alloc_token = 0; alloc_token <= kMaxAllocToken.value();
       alloc_token++) {
    std::ignore = Allocator(AllocToken(alloc_token));
  }
}

}  // namespace allocator_shim

#endif  // PA_BUILDFLAG(IS_APPLE)

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
