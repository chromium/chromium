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
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_internal.h"
#include "partition_alloc/shim/allocator_shim_internals.h"

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#endif

using allocator_shim::AllocatorDispatch;

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
  alignas(T) uint8_t instance_buffer_[sizeof(T)] = {0};
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
    opts.star_scan_quarantine = partition_alloc::PartitionOptions::kAllowed;
    opts.backup_ref_ptr = partition_alloc::PartitionOptions::kDisabled;
    auto* new_root = new (buffer) partition_alloc::PartitionRoot(opts);

    return new_root;
  }
};

LeakySingleton<partition_alloc::PartitionRoot, MainPartitionConstructor> g_root
    PA_CONSTINIT = {};
partition_alloc::PartitionRoot* Allocator() {
  return g_root.Get();
}

// Original g_root_ if it was replaced by ConfigurePartitions().
std::atomic<partition_alloc::PartitionRoot*> g_original_root(nullptr);

std::atomic<bool> g_roots_finalized = false;

partition_alloc::PartitionRoot* OriginalAllocator() {
  return g_original_root.load(std::memory_order_relaxed);
}

bool AllocatorConfigurationFinalized() {
  return g_roots_finalized.load();
}

template <partition_alloc::AllocFlags flags>
void* AllocateAlignedMemory(size_t alignment, size_t size) {
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
    return Allocator()->AllocInline<flags>(size);
  }

  return Allocator()->AlignedAllocInline<flags>(alignment, size);
}

}  // namespace

namespace allocator_shim::internal {

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags,
                                      base_free_flags>::Malloc(size_t size,
                                                               void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return Allocator()->AllocInline<base_alloc_flags>(size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    MallocUnchecked(size_t size, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return Allocator()
      ->AllocInline<base_alloc_flags |
                    partition_alloc::AllocFlags::kReturnNull>(size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags,
                                      base_free_flags>::Calloc(size_t n,
                                                               size_t size,
                                                               void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  const size_t total =
      partition_alloc::internal::base::CheckMul(n, size).ValueOrDie();
  return Allocator()
      ->AllocInline<base_alloc_flags | partition_alloc::AllocFlags::kZeroFill>(
          total);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    Memalign(size_t alignment, size_t size, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory<base_alloc_flags>(alignment, size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    AlignedAlloc(size_t size, size_t alignment, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory<base_alloc_flags>(alignment, size);
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    AlignedAllocUnchecked(size_t size, size_t alignment, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory<base_alloc_flags |
                               partition_alloc::AllocFlags::kReturnNull>(
      alignment, size);
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
                   void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  void* new_ptr = nullptr;
  if (size > 0) {
    new_ptr = AllocateAlignedMemory<base_alloc_flags>(alignment, size);
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
    memcpy(new_ptr, address, copy_size);

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
                            void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  void* new_ptr = nullptr;
  if (size > 0) {
    new_ptr = AllocateAlignedMemory<base_alloc_flags |
                                    partition_alloc::AllocFlags::kReturnNull>(
        alignment, size);
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
    memcpy(new_ptr, address, copy_size);

    partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
        address);
  }
  return new_ptr;
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags,
                                      base_free_flags>::Realloc(void* address,
                                                                size_t size,
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

  return Allocator()->Realloc<base_alloc_flags, base_free_flags>(address, size,
                                                                 "");
}

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void* PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    ReallocUnchecked(void* address, size_t size, void* context) {
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

  return Allocator()
      ->Realloc<base_alloc_flags | partition_alloc::AllocFlags::kReturnNull>(
          address, size, "");
}

#if PA_BUILDFLAG(IS_CAST_ANDROID)
extern "C" {
void __real_free(void*);
}       // extern "C"
#endif  // PA_BUILDFLAG(IS_CAST_ANDROID)

// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
PA_ALWAYS_INLINE void
PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::Free(
    void* object,
    void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
#if PA_BUILDFLAG(IS_APPLE)
  // TODO(bartekn): Add MTE unmasking here (and below).
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(object)) &&
      object) [[unlikely]] {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return free(object);
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
    return __real_free(object);
  }
#endif  // PA_BUILDFLAG(IS_CAST_ANDROID)

  partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
      object);
}

#if PA_BUILDFLAG(IS_APPLE)
// Normal free() path on Apple OSes:
// 1. size = GetSizeEstimate(ptr);
// 2. if (size) FreeDefiniteSize(ptr, size)
//
// So we don't need to re-check that the pointer is owned in Free(), and we
// can use the size.
// static
template <partition_alloc::AllocFlags base_alloc_flags,
          partition_alloc::FreeFlags base_free_flags>
void PartitionAllocFunctionsInternal<base_alloc_flags, base_free_flags>::
    FreeDefiniteSize(void* address, size_t size, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  // TODO(lizeb): Optimize PartitionAlloc to use the size information. This is
  // still useful though, as we avoid double-checking that the address is owned.
  partition_alloc::PartitionRoot::FreeInlineInUnknownRoot<base_free_flags>(
      address);
}
#endif  // PA_BUILDFLAG(IS_APPLE)

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
  const size_t size =
      partition_alloc::PartitionRoot::GetUsableSizeWithMac11MallocSizeHack(
          address);
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
  return Allocator()->AllocationCapacityFromRequestedSize(size);
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
    results[i] = Malloc(size, nullptr);
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
    Free(to_be_freed[i], nullptr);
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
template class PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
    PartitionAllocFunctionsInternal<partition_alloc::AllocFlags::kNoHooks,
                                    partition_alloc::FreeFlags::kNoHooks>;
// Explicitly instantiate `PartitionAllocWithAdvancedChecksFunctions`.
template class PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
    PartitionAllocFunctionsInternal<
        partition_alloc::AllocFlags::kNoHooks,
        partition_alloc::FreeFlags::kNoHooks |
            partition_alloc::FreeFlags::kZap |
            partition_alloc::FreeFlags::kSchedulerLoopQuarantine>;

// static
bool PartitionAllocMalloc::AllocatorConfigurationFinalized() {
  return ::AllocatorConfigurationFinalized();
}

// static
partition_alloc::PartitionRoot* PartitionAllocMalloc::Allocator() {
  return ::Allocator();
}

// static
partition_alloc::PartitionRoot* PartitionAllocMalloc::OriginalAllocator() {
  return ::OriginalAllocator();
}

}  // namespace allocator_shim::internal

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace allocator_shim {

void EnablePartitionAllocMemoryReclaimer() {
  // Unlike other partitions, Allocator() does not register its PartitionRoot to
  // the memory reclaimer, because doing so may allocate memory. Thus, the
  // registration to the memory reclaimer has to be done some time later, when
  // the main root is fully configured.
  ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
      Allocator());

  // There is only one PartitionAlloc-Everywhere partition at the moment. Any
  // additional partitions will be created in ConfigurePartitions() and
  // registered for memory reclaimer there.
  PA_DCHECK(!AllocatorConfigurationFinalized());
  PA_DCHECK(OriginalAllocator() == nullptr);
}

void ConfigurePartitions(
    EnableBrp enable_brp,
    EnableMemoryTagging enable_memory_tagging,
    partition_alloc::TagViolationReportingMode memory_tagging_reporting_mode,
    BucketDistribution distribution,
    SchedulerLoopQuarantine scheduler_loop_quarantine,
    size_t scheduler_loop_quarantine_branch_capacity_in_bytes,
    ZappingByFreeFlags zapping_by_free_flags,
    UsePoolOffsetFreelists use_pool_offset_freelists,
    UseSmallSingleSlotSpans use_small_single_slot_spans) {
  // Calling Get() is actually important, even if the return value isn't
  // used, because it has a side effect of initializing the variable, if it
  // wasn't already.
  auto* current_root = g_root.Get();

  // We've been bitten before by using a static local when initializing a
  // partition. For synchronization, static local variables call into the
  // runtime on Windows, which may not be ready to handle it, if the path is
  // invoked on an allocation during the runtime initialization.
  // ConfigurePartitions() is invoked explicitly from Chromium code, so this
  // shouldn't bite us here. Mentioning just in case we move this code earlier.
  static partition_alloc::internal::base::NoDestructor<
      partition_alloc::PartitionAllocator>
      new_main_allocator([&] {
        partition_alloc::PartitionOptions opts;
        // The caller of ConfigurePartitions() will decide whether this or
        // another partition will have the thread cache enabled, by calling
        // EnableThreadCacheIfSupported().
        opts.thread_cache = partition_alloc::PartitionOptions::kDisabled;
        opts.star_scan_quarantine = partition_alloc::PartitionOptions::kAllowed;
        opts.backup_ref_ptr =
            enable_brp ? partition_alloc::PartitionOptions::kEnabled
                       : partition_alloc::PartitionOptions::kDisabled;
        opts.zapping_by_free_flags =
            zapping_by_free_flags
                ? partition_alloc::PartitionOptions::kEnabled
                : partition_alloc::PartitionOptions::kDisabled;
        opts.scheduler_loop_quarantine =
            scheduler_loop_quarantine
                ? partition_alloc::PartitionOptions::kEnabled
                : partition_alloc::PartitionOptions::kDisabled;
        opts.scheduler_loop_quarantine_branch_capacity_in_bytes =
            scheduler_loop_quarantine_branch_capacity_in_bytes;
        opts.memory_tagging = {
            .enabled = enable_memory_tagging
                           ? partition_alloc::PartitionOptions::kEnabled
                           : partition_alloc::PartitionOptions::kDisabled,
            .reporting_mode = memory_tagging_reporting_mode};
        opts.use_pool_offset_freelists =
            use_pool_offset_freelists
                ? partition_alloc::PartitionOptions::kEnabled
                : partition_alloc::PartitionOptions::kDisabled;
        opts.use_small_single_slot_spans =
            use_small_single_slot_spans
                ? partition_alloc::PartitionOptions::kEnabled
                : partition_alloc::PartitionOptions::kDisabled;
        return opts;
      }());
  partition_alloc::PartitionRoot* new_root = new_main_allocator->root();

  // Now switch traffic to the new partition.
  g_original_root = current_root;
  g_root.Replace(new_root);

  // Purge memory, now that the traffic to the original partition is cut off.
  current_root->PurgeMemory(
      partition_alloc::PurgeFlags::kDecommitEmptySlotSpans |
      partition_alloc::PurgeFlags::kDiscardUnusedSystemPages);

  switch (distribution) {
    case BucketDistribution::kNeutral:
      // We start in the 'default' case.
      break;
    case BucketDistribution::kDenser:
      new_root->SwitchToDenserBucketDistribution();
      break;
  }

  PA_CHECK(!g_roots_finalized.exchange(true));  // Ensure configured once.
}

// No synchronization provided: `PartitionRoot.flags` is only written
// to in `PartitionRoot::Init()`.
uint32_t GetMainPartitionRootExtrasSize() {
#if PA_CONFIG(EXTRAS_REQUIRED)
  return g_root.Get()->settings.extras_size;
#else
  return 0;
#endif  // PA_CONFIG(EXTRAS_REQUIRED)
}

void AdjustDefaultAllocatorForForeground() {
  Allocator()->AdjustForForeground();
}

void AdjustDefaultAllocatorForBackground() {
  Allocator()->AdjustForBackground();
}

}  // namespace allocator_shim

#if !PA_BUILDFLAG( \
    ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)
const AllocatorDispatch AllocatorDispatch::default_dispatch =
    internal::kPartitionAllocDispatch;
#endif  // !PA_BUILDFLAG(ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)

// Intercept diagnostics symbols as well, even though they are not part of the
// unified shim layer.
//
// TODO(lizeb): Implement the ones that doable.

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
  Allocator()->DumpStats("malloc", true, &allocator_dumper);

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
  std::ignore = Allocator();
}

}  // namespace allocator_shim

#endif  // PA_BUILDFLAG(IS_APPLE)

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
