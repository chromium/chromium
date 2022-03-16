// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"

#include <atomic>
#include <cstddef>
#include <map>
#include <string>
#include <tuple>

#include "base/allocator/allocator_shim_internals.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/nonscannable_memory.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#endif

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
#include <windows.h>
#endif

using base::allocator::AllocatorDispatch;

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

  ALWAYS_INLINE T* Get() {
    auto* instance = instance_.load(std::memory_order_acquire);
    if (LIKELY(instance))
      return instance;

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
  alignas(T) uint8_t instance_buffer_[sizeof(T)];
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
  if (instance)
    return instance;

  instance = Constructor::New(reinterpret_cast<void*>(instance_buffer_));
  instance_.store(instance, std::memory_order_release);

  return instance;
}

class MainPartitionConstructor {
 public:
  static base::ThreadSafePartitionRoot* New(void* buffer) {
    constexpr base::PartitionOptions::ThreadCache thread_cache =
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // Additional partitions may be created in ConfigurePartitions(). Since
        // only one partition can have thread cache enabled, postpone the
        // decision to turn the thread cache on until after that call.
        // TODO(bartekn): Enable it here by default, once the "split-only" mode
        // is no longer needed.
        base::PartitionOptions::ThreadCache::kDisabled;
#else   // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // Other tests, such as the ThreadCache tests create a thread cache,
        // and only one is supported at a time.
        base::PartitionOptions::ThreadCache::kDisabled;
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    auto* new_root = new (buffer) base::ThreadSafePartitionRoot({
        base::PartitionOptions::AlignedAlloc::kAllowed,
        thread_cache,
        base::PartitionOptions::Quarantine::kAllowed,
        base::PartitionOptions::Cookie::kAllowed,
        base::PartitionOptions::BackupRefPtr::kDisabled,
        base::PartitionOptions::UseConfigurablePool::kNo,
    });

    return new_root;
  }
};

LeakySingleton<base::ThreadSafePartitionRoot, MainPartitionConstructor> g_root
    CONSTINIT = {};
base::ThreadSafePartitionRoot* Allocator() {
  return g_root.Get();
}

// Original g_root_ if it was replaced by ConfigurePartitions().
std::atomic<base::ThreadSafePartitionRoot*> g_original_root(nullptr);

class AlignedPartitionConstructor {
 public:
  static base::ThreadSafePartitionRoot* New(void* buffer) {
    return g_root.Get();
  }
};

LeakySingleton<base::ThreadSafePartitionRoot, AlignedPartitionConstructor>
    g_aligned_root CONSTINIT = {};

base::ThreadSafePartitionRoot* OriginalAllocator() {
  return g_original_root.load(std::memory_order_relaxed);
}

base::ThreadSafePartitionRoot* AlignedAllocator() {
  return g_aligned_root.Get();
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
bool IsRunning32bitEmulatedOnArm64() {
  using IsWow64Process2Function = decltype(&IsWow64Process2);

  IsWow64Process2Function is_wow64_process2 =
      reinterpret_cast<IsWow64Process2Function>(::GetProcAddress(
          ::GetModuleHandleA("kernel32.dll"), "IsWow64Process2"));
  if (!is_wow64_process2)
    return false;
  USHORT process_machine;
  USHORT native_machine;
  bool retval = is_wow64_process2(::GetCurrentProcess(), &process_machine,
                                  &native_machine);
  if (!retval)
    return false;
  if (native_machine == IMAGE_FILE_MACHINE_ARM64)
    return true;
  return false;
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// The number of bytes to add to every allocation. Ordinarily zero, but set to 8
// when emulating an x86 on ARM64 to avoid a bug in the Windows x86 emulator.
size_t g_extra_bytes;
#endif  // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)

// TODO(brucedawson): Remove this when https://crbug.com/1151455 is fixed.
ALWAYS_INLINE size_t MaybeAdjustSize(size_t size) {
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
  return base::CheckAdd(size, g_extra_bytes).ValueOrDie();
#else   // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
  return size;
#endif  // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
}

void* AllocateAlignedMemory(size_t alignment, size_t size) {
  // Memory returned by the regular allocator *always* respects |kAlignment|,
  // which is a power of two, and any valid alignment is also a power of two. So
  // we can directly fulfill these requests with the main allocator.
  //
  // This has several advantages:
  // - The thread cache is supported on the main partition
  // - Reduced fragmentation
  // - Better coverage for MiraclePtr variants requiring extras
  //
  // There are several call sites in Chromium where base::AlignedAlloc is called
  // with a small alignment. Some may be due to overly-careful code, some are
  // because the client code doesn't know the required alignment at compile
  // time.
  //
  // Note that all "AlignedFree()" variants (_aligned_free() on Windows for
  // instance) directly call PartitionFree(), so there is no risk of
  // mismatch. (see below the default_dispatch definition).
  if (alignment <= base::kAlignment) {
    // This is mandated by |posix_memalign()| and friends, so should never fire.
    PA_CHECK(base::bits::IsPowerOfTwo(alignment));
    // TODO(bartekn): See if the compiler optimizes branches down the stack on
    // Mac, where PartitionPageSize() isn't constexpr.
    return Allocator()->AllocWithFlagsNoHooks(0, size,
                                              base::PartitionPageSize());
  }

  return AlignedAllocator()->AlignedAllocWithFlags(
      partition_alloc::AllocFlags::kNoHooks, alignment, size);
}

}  // namespace

namespace base {
namespace internal {

namespace {
#if BUILDFLAG(IS_APPLE)
int g_alloc_flags = 0;
#else
constexpr int g_alloc_flags = 0;
#endif
}  // namespace

void PartitionAllocSetCallNewHandlerOnMallocFailure(bool value) {
#if BUILDFLAG(IS_APPLE)
  // We generally prefer to always crash rather than returning nullptr for
  // OOM. However, on some macOS releases, we have to locally allow it due to
  // weirdness in OS code. See https://crbug.com/654695 for details.
  //
  // Apple only since it's not needed elsewhere, and there is a performance
  // penalty.

  if (value)
    g_alloc_flags = 0;
  else
    g_alloc_flags = partition_alloc::AllocFlags::kReturnNull;
#endif
}

void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context) {
  ScopedDisallowAllocations guard{};
  return Allocator()->AllocWithFlagsNoHooks(
      0 | g_alloc_flags, MaybeAdjustSize(size), PartitionPageSize());
}

void* PartitionMallocUnchecked(const AllocatorDispatch*,
                               size_t size,
                               void* context) {
  ScopedDisallowAllocations guard{};
  return Allocator()->AllocWithFlagsNoHooks(
      partition_alloc::AllocFlags::kReturnNull | g_alloc_flags,
      MaybeAdjustSize(size), PartitionPageSize());
}

void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context) {
  ScopedDisallowAllocations guard{};
  const size_t total = base::CheckMul(n, MaybeAdjustSize(size)).ValueOrDie();
  return Allocator()->AllocWithFlagsNoHooks(
      partition_alloc::AllocFlags::kZeroFill | g_alloc_flags, total,
      PartitionPageSize());
}

void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context) {
  ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory(alignment, size);
}

void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                            size_t size,
                            size_t alignment,
                            void* context) {
  ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory(alignment, size);
}

// aligned_realloc documentation is
// https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/reference/aligned-realloc
// TODO(tasak): Expand the given memory block to the given size if possible.
// This realloc always free the original memory block and allocates a new memory
// block.
// TODO(tasak): Implement PartitionRoot<thread_safe>::AlignedReallocWithFlags
// and use it.
void* PartitionAlignedRealloc(const AllocatorDispatch* dispatch,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  ScopedDisallowAllocations guard{};
  void* new_ptr = nullptr;
  if (size > 0) {
    size = MaybeAdjustSize(size);
    new_ptr = AllocateAlignedMemory(alignment, size);
  } else {
    // size == 0 and address != null means just "free(address)".
    if (address)
      base::ThreadSafePartitionRoot::FreeNoHooks(address);
  }
  // The original memory block (specified by address) is unchanged if ENOMEM.
  if (!new_ptr)
    return nullptr;
  // TODO(tasak): Need to compare the new alignment with the address' alignment.
  // If the two alignments are not the same, need to return nullptr with EINVAL.
  if (address) {
    size_t usage = base::ThreadSafePartitionRoot::GetUsableSize(address);
    size_t copy_size = usage > size ? size : usage;
    memcpy(new_ptr, address, copy_size);

    base::ThreadSafePartitionRoot::FreeNoHooks(address);
  }
  return new_ptr;
}

void* PartitionRealloc(const AllocatorDispatch*,
                       void* address,
                       size_t size,
                       void* context) {
  ScopedDisallowAllocations guard{};
#if BUILDFLAG(IS_APPLE)
  if (UNLIKELY(!base::IsManagedByPartitionAlloc(
                   reinterpret_cast<uintptr_t>(address)) &&
               address)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `realloc` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return realloc(address, size);
  }
#endif  // BUILDFLAG(IS_APPLE)

  return Allocator()->ReallocWithFlags(
      partition_alloc::AllocFlags::kNoHooks | g_alloc_flags, address,
      MaybeAdjustSize(size), "");
}

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_CHROMECAST)
extern "C" {
void __real_free(void*);
}  // extern "C"
#endif

void PartitionFree(const AllocatorDispatch*, void* object, void* context) {
  ScopedDisallowAllocations guard{};
#if BUILDFLAG(IS_APPLE)
  // TODO(bartekn): Add MTE unmasking here (and below).
  if (UNLIKELY(!base::IsManagedByPartitionAlloc(
                   reinterpret_cast<uintptr_t>(object)) &&
               object)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return free(object);
  }
#endif  // BUILDFLAG(IS_APPLE)

  // On Chromecast, there is at least one case where a system malloc() pointer
  // can be passed to PartitionAlloc's free(). If we don't own the pointer, pass
  // it along. This should not have a runtime cost vs regular Android, since on
  // Android we have a PA_CHECK() rather than the branch here.
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_CHROMECAST)
  if (UNLIKELY(!base::IsManagedByPartitionAlloc(
                   reinterpret_cast<uintptr_t>(object)) &&
               object)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free()`, which is `__real_free()`
    // here.
    return __real_free(object);
  }
#endif

  base::ThreadSafePartitionRoot::FreeNoHooks(object);
}

#if BUILDFLAG(IS_APPLE)
// Normal free() path on Apple OSes:
// 1. size = GetSizeEstimate(ptr);
// 2. if (size) FreeDefiniteSize(ptr, size)
//
// So we don't need to re-check that the pointer is owned in Free(), and we
// can use the size.
void PartitionFreeDefiniteSize(const AllocatorDispatch*,
                               void* address,
                               size_t size,
                               void* context) {
  ScopedDisallowAllocations guard{};
  // TODO(lizeb): Optimize PartitionAlloc to use the size information. This is
  // still useful though, as we avoid double-checking that the address is owned.
  base::ThreadSafePartitionRoot::FreeNoHooks(address);
}
#endif  // BUILDFLAG(IS_APPLE)

size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context) {
  // This is used to implement malloc_usable_size(3). Per its man page, "if ptr
  // is NULL, 0 is returned".
  if (!address)
    return 0;

#if BUILDFLAG(IS_APPLE)
  if (!base::IsManagedByPartitionAlloc(reinterpret_cast<uintptr_t>(address))) {
    // The object pointed to by `address` is not allocated by the
    // PartitionAlloc.  The return value `0` means that the pointer does not
    // belong to this malloc zone.
    return 0;
  }
#endif  // BUILDFLAG(IS_APPLE)

  // TODO(lizeb): Returns incorrect values for aligned allocations.
  const size_t size = base::ThreadSafePartitionRoot::GetUsableSize(address);
#if BUILDFLAG(IS_APPLE)
  // The object pointed to by `address` is allocated by the PartitionAlloc.
  // So, this function must not return zero so that the malloc zone dispatcher
  // finds the appropriate malloc zone.
  PA_DCHECK(size);
#endif  // BUILDFLAG(IS_APPLE)
  return size;
}

unsigned PartitionBatchMalloc(const AllocatorDispatch*,
                              size_t size,
                              void** results,
                              unsigned num_requested,
                              void* context) {
  // No real batching: we could only acquire the lock once for instance, keep it
  // simple for now.
  for (unsigned i = 0; i < num_requested; i++) {
    // No need to check the results, we crash if it fails.
    results[i] = PartitionMalloc(nullptr, size, nullptr);
  }

  // Either all succeeded, or we crashed.
  return num_requested;
}

void PartitionBatchFree(const AllocatorDispatch*,
                        void** to_be_freed,
                        unsigned num_to_be_freed,
                        void* context) {
  // No real batching: we could only acquire the lock once for instance, keep it
  // simple for now.
  for (unsigned i = 0; i < num_to_be_freed; i++) {
    PartitionFree(nullptr, to_be_freed[i], nullptr);
  }
}

// static
ThreadSafePartitionRoot* PartitionAllocMalloc::Allocator() {
  return ::Allocator();
}

// static
ThreadSafePartitionRoot* PartitionAllocMalloc::OriginalAllocator() {
  return ::OriginalAllocator();
}

// static
ThreadSafePartitionRoot* PartitionAllocMalloc::AlignedAllocator() {
  return ::AlignedAllocator();
}

}  // namespace internal
}  // namespace base

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base {
namespace allocator {

void EnablePartitionAllocMemoryReclaimer() {
  // Unlike other partitions, Allocator() and AlignedAllocator() do not register
  // their PartitionRoots to the memory reclaimer, because doing so may allocate
  // memory. Thus, the registration to the memory reclaimer has to be done
  // some time later, when the main root is fully configured.
  // TODO(bartekn): Aligned allocator can use the regular initialization path.
  ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
      Allocator());
  auto* original_root = OriginalAllocator();
  if (original_root)
    ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
        original_root);
  if (AlignedAllocator() != Allocator()) {
    ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
        AlignedAllocator());
  }
}

alignas(base::ThreadSafePartitionRoot) uint8_t
    g_allocator_buffer_for_new_main_partition[sizeof(
        base::ThreadSafePartitionRoot)];

alignas(base::ThreadSafePartitionRoot) uint8_t
    g_allocator_buffer_for_aligned_alloc_partition[sizeof(
        base::ThreadSafePartitionRoot)];

void ConfigurePartitions(
    EnableBrp enable_brp,
    SplitMainPartition split_main_partition,
    UseDedicatedAlignedPartition use_dedicated_aligned_partition,
    AlternateBucketDistribution use_alternate_bucket_distribution) {
  // BRP cannot be enabled without splitting the main partition. Furthermore, in
  // the "before allocation" mode, it can't be enabled without further splitting
  // out the aligned partition.
  PA_CHECK(!enable_brp || split_main_partition);
#if !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
  PA_CHECK(!enable_brp || use_dedicated_aligned_partition);
#endif
  // Can't split out the aligned partition, without splitting the main one.
  PA_CHECK(!use_dedicated_aligned_partition || split_main_partition);

  static bool configured = false;
  PA_CHECK(!configured);
  configured = true;

  // Calling Get() is actually important, even if the return values weren't
  // used, because it has a side effect of initializing the variables, if they
  // weren't already.
  auto* current_root = g_root.Get();
  auto* current_aligned_root = g_aligned_root.Get();

  if (!split_main_partition) {
    if (!use_alternate_bucket_distribution) {
      current_root->SwitchToDenserBucketDistribution();
      current_aligned_root->SwitchToDenserBucketDistribution();
    }
    PA_DCHECK(!enable_brp);
    PA_DCHECK(!use_dedicated_aligned_partition);
    PA_DCHECK(!current_root->with_thread_cache);
    return;
  }

  auto* new_root =
      new (g_allocator_buffer_for_new_main_partition) ThreadSafePartitionRoot({
          !use_dedicated_aligned_partition
              ? base::PartitionOptions::AlignedAlloc::kAllowed
              : base::PartitionOptions::AlignedAlloc::kDisallowed,
          base::PartitionOptions::ThreadCache::kDisabled,
          base::PartitionOptions::Quarantine::kAllowed,
          base::PartitionOptions::Cookie::kAllowed,
          enable_brp ? base::PartitionOptions::BackupRefPtr::kEnabled
                     : base::PartitionOptions::BackupRefPtr::kDisabled,
          base::PartitionOptions::UseConfigurablePool::kNo,
      });

  base::ThreadSafePartitionRoot* new_aligned_root;
  if (use_dedicated_aligned_partition) {
    // TODO(bartekn): Use the original root instead of creating a new one. It'd
    // result in one less partition, but come at a cost of commingling types.
    new_aligned_root = new (g_allocator_buffer_for_aligned_alloc_partition)
        ThreadSafePartitionRoot({
            base::PartitionOptions::AlignedAlloc::kAllowed,
            base::PartitionOptions::ThreadCache::kDisabled,
            base::PartitionOptions::Quarantine::kAllowed,
            base::PartitionOptions::Cookie::kAllowed,
            base::PartitionOptions::BackupRefPtr::kDisabled,
            base::PartitionOptions::UseConfigurablePool::kNo,
        });
  } else {
    // The new main root can also support AlignedAlloc.
    new_aligned_root = new_root;
  }

  // Now switch traffic to the new partitions.
  g_aligned_root.Replace(new_aligned_root);
  g_root.Replace(new_root);

  // g_original_root has to be set after g_root, because other code doesn't
  // handle well both pointing to the same root.
  // TODO(bartekn): Reorder, once handled well. It isn't ideal for one
  // partition to be invisible temporarily.
  g_original_root = current_root;

  // No need for g_original_aligned_root, because in cases where g_aligned_root
  // is replaced, it must've been g_original_root.
  PA_CHECK(current_aligned_root == g_original_root);

  // Purge memory, now that the traffic to the original partition is cut off.
  current_root->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                            PurgeFlags::kDiscardUnusedSystemPages);

  if (!use_alternate_bucket_distribution) {
    g_root.Get()->SwitchToDenserBucketDistribution();
    g_aligned_root.Get()->SwitchToDenserBucketDistribution();
  }
}

#if defined(PA_ALLOW_PCSCAN)
void EnablePCScan(base::internal::PCScan::InitConfig config) {
  internal::PCScan::Initialize(config);

  internal::PCScan::RegisterScannableRoot(Allocator());
  if (OriginalAllocator() != nullptr)
    internal::PCScan::RegisterScannableRoot(OriginalAllocator());
  if (Allocator() != AlignedAllocator())
    internal::PCScan::RegisterScannableRoot(AlignedAllocator());

  internal::NonScannableAllocator::Instance().NotifyPCScanEnabled();
  internal::NonQuarantinableAllocator::Instance().NotifyPCScanEnabled();
}
#endif  // defined(PA_ALLOW_PCSCAN)

#if BUILDFLAG(IS_WIN)
// Call this as soon as possible during startup.
void ConfigurePartitionAlloc() {
#if defined(ARCH_CPU_X86)
  if (IsRunning32bitEmulatedOnArm64())
    g_extra_bytes = 8;
#endif  // defined(ARCH_CPU_X86)
}
#endif  // BUILDFLAG(IS_WIN)
}  // namespace allocator
}  // namespace base

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &base::internal::PartitionMalloc,           // alloc_function
    &base::internal::PartitionMallocUnchecked,  // alloc_unchecked_function
    &base::internal::PartitionCalloc,    // alloc_zero_initialized_function
    &base::internal::PartitionMemalign,  // alloc_aligned_function
    &base::internal::PartitionRealloc,   // realloc_function
    &base::internal::PartitionFree,      // free_function
    &base::internal::PartitionGetSizeEstimate,  // get_size_estimate_function
    &base::internal::PartitionBatchMalloc,      // batch_malloc_function
    &base::internal::PartitionBatchFree,        // batch_free_function
#if BUILDFLAG(IS_APPLE)
    // On Apple OSes, free_definite_size() is always called from free(), since
    // get_size_estimate() is used to determine whether an allocation belongs to
    // the current zone. It makes sense to optimize for it.
    &base::internal::PartitionFreeDefiniteSize,
#else
    nullptr,  // free_definite_size_function
#endif
    &base::internal::PartitionAlignedAlloc,    // aligned_malloc_function
    &base::internal::PartitionAlignedRealloc,  // aligned_realloc_function
    &base::internal::PartitionFree,            // aligned_free_function
    nullptr,                                   // next
};

// Intercept diagnostics symbols as well, even though they are not part of the
// unified shim layer.
//
// TODO(lizeb): Implement the ones that doable.

extern "C" {

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

SHIM_ALWAYS_EXPORT void malloc_stats(void) __THROW {}

SHIM_ALWAYS_EXPORT int mallopt(int cmd, int value) __THROW {
  return 0;
}

#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
  base::SimplePartitionStatsDumper allocator_dumper;
  Allocator()->DumpStats("malloc", true, &allocator_dumper);
  // TODO(bartekn): Dump OriginalAllocator() into "malloc" as well.

  base::SimplePartitionStatsDumper aligned_allocator_dumper;
  if (AlignedAllocator() != Allocator()) {
    AlignedAllocator()->DumpStats("posix_memalign", true,
                                  &aligned_allocator_dumper);
  }

  // Dump stats for nonscannable and nonquarantinable allocators.
  auto& nonscannable_allocator =
      base::internal::NonScannableAllocator::Instance();
  base::SimplePartitionStatsDumper nonscannable_allocator_dumper;
  if (auto* nonscannable_root = nonscannable_allocator.root())
    nonscannable_root->DumpStats("malloc", true,
                                 &nonscannable_allocator_dumper);
  auto& nonquarantinable_allocator =
      base::internal::NonQuarantinableAllocator::Instance();
  base::SimplePartitionStatsDumper nonquarantinable_allocator_dumper;
  if (auto* nonquarantinable_root = nonquarantinable_allocator.root())
    nonquarantinable_root->DumpStats("malloc", true,
                                     &nonquarantinable_allocator_dumper);

  struct mallinfo info = {0};
  info.arena = 0;  // Memory *not* allocated with mmap().

  // Memory allocated with mmap(), aka virtual size.
  info.hblks = allocator_dumper.stats().total_mmapped_bytes +
               aligned_allocator_dumper.stats().total_mmapped_bytes +
               nonscannable_allocator_dumper.stats().total_mmapped_bytes +
               nonquarantinable_allocator_dumper.stats().total_mmapped_bytes;
  // Resident bytes.
  info.hblkhd = allocator_dumper.stats().total_resident_bytes +
                aligned_allocator_dumper.stats().total_resident_bytes +
                nonscannable_allocator_dumper.stats().total_resident_bytes +
                nonquarantinable_allocator_dumper.stats().total_resident_bytes;
  // Allocated bytes.
  info.uordblks = allocator_dumper.stats().total_active_bytes +
                  aligned_allocator_dumper.stats().total_active_bytes +
                  nonscannable_allocator_dumper.stats().total_active_bytes +
                  nonquarantinable_allocator_dumper.stats().total_active_bytes;

  return info;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // extern "C"

#if BUILDFLAG(IS_APPLE)

namespace base {
namespace allocator {

void InitializeDefaultAllocatorPartitionRoot() {
  // On OS_APPLE, the initialization of PartitionRoot uses memory allocations
  // internally, e.g. __builtin_available, and it's not easy to avoid it.
  // Thus, we initialize the PartitionRoot with using the system default
  // allocator before we intercept the system default allocator.
  std::ignore = Allocator();
}

}  // namespace allocator
}  // namespace base

#endif  // BUILDFLAG(IS_APPLE)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
