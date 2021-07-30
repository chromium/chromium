// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"

#include <atomic>
#include <cstddef>

#include "base/allocator/allocator_shim_internals.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/memory/nonscannable_memory.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <malloc.h>
#endif

#if defined(OS_WIN) && defined(ARCH_CPU_X86)
#include <windows.h>
#endif

using base::allocator::AllocatorDispatch;

namespace {

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
  //
  // Lock.
  bool expected = false;
  // Semantically equivalent to base::Lock::Acquire().
  while (!initialization_lock_.compare_exchange_strong(
      expected, true, std::memory_order_acquire, std::memory_order_acquire)) {
    expected = false;
  }

  T* instance = instance_.load(std::memory_order_relaxed);
  // Someone beat us.
  if (instance) {
    // Unlock.
    initialization_lock_.store(false, std::memory_order_release);
    return instance;
  }

  instance = Constructor::New(reinterpret_cast<void*>(instance_buffer_));
  instance_.store(instance, std::memory_order_release);

  // Unlock.
  initialization_lock_.store(false, std::memory_order_release);

  return instance;
}

class MainPartitionConstructor {
 public:
  static base::ThreadSafePartitionRoot* New(void* buffer) {
    auto* new_root = new (buffer) base::ThreadSafePartitionRoot({
#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
      base::PartitionOptions::AlignedAlloc::kDisallowed,
#else
      base::PartitionOptions::AlignedAlloc::kAllowed,
#endif
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    !BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)
          base::PartitionOptions::ThreadCache::kEnabled,
#elif BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)
          // With ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL, this partition is only
          // temporary until BackupRefPtr is re-configured at run-time. Leave
          // the ability to have a thread cache to the main partition. (Note
          // that ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL implies that
          // USE_BACKUP_REF_PTR is true.)
          base::PartitionOptions::ThreadCache::kDisabled,
#else
      // Other tests, such as the ThreadCache tests create a thread cache, and
      // only one is supported at a time.
      base::PartitionOptions::ThreadCache::kDisabled,
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // !BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)
          base::PartitionOptions::Quarantine::kAllowed,
#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
          base::PartitionOptions::Cookies::kAllowed,
#else
          base::PartitionOptions::Cookies::kDisallowed,
#endif
#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC) || \
    BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
          base::PartitionOptions::RefCount::kAllowed,
#else
          base::PartitionOptions::RefCount::kDisallowed,
#endif
    });

    return new_root;
  }
};

#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
class AlignedPartitionConstructor {
 public:
  static base::ThreadSafePartitionRoot* New(void* buffer) {
    // Since the general-purpose allocator uses the thread cache, this one
    // cannot.
    auto* new_root =
        new (buffer) base::ThreadSafePartitionRoot(base::PartitionOptions {
          base::PartitionOptions::AlignedAlloc::kAllowed,
              base::PartitionOptions::ThreadCache::kDisabled,
              base::PartitionOptions::Quarantine::kAllowed,
              base::PartitionOptions::Cookies::kDisallowed,
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
              // Given the outer #if, this is possible only when DCHECK_IS_ON().
              base::PartitionOptions::RefCount::kAllowed,
#else
            base::PartitionOptions::RefCount::kDisallowed,
#endif
        });
    return new_root;
  }
};

LeakySingleton<base::ThreadSafePartitionRoot, AlignedPartitionConstructor>
    g_aligned_root CONSTINIT = {};
#endif  // BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)

// Original g_root_ if it was replaced by ConfigurePartitionRefCountSupport().
std::atomic<base::ThreadSafePartitionRoot*> g_original_root_(nullptr);

LeakySingleton<base::ThreadSafePartitionRoot, MainPartitionConstructor> g_root
    CONSTINIT = {};
base::ThreadSafePartitionRoot* Allocator() {
  return g_root.Get();
}

base::ThreadSafePartitionRoot* OriginalAllocator() {
  return g_original_root_.load(std::memory_order_relaxed);
}

base::ThreadSafePartitionRoot* AlignedAllocator() {
#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
  return g_aligned_root.Get();
#else   // BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
  return Allocator();
#endif  // BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
}

#if defined(OS_WIN) && defined(ARCH_CPU_X86)
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
#endif  // defined(OS_WIN) && defined(ARCH_CPU_X86)

// TODO(brucedawson): Remove this when https://crbug.com/1151455 is fixed.
ALWAYS_INLINE size_t MaybeAdjustSize(size_t size) {
#if defined(OS_WIN) && defined(ARCH_CPU_X86)
  return base::CheckAdd(size, g_extra_bytes).ValueOrDie();
#else   // defined(OS_WIN) && defined(ARCH_CPU_X86)
  return size;
#endif  // defined(OS_WIN) && defined(ARCH_CPU_X86)
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
    return Allocator()->AllocFlagsNoHooks(0, size, base::PartitionPageSize());
  }

  return AlignedAllocator()->AlignedAllocFlags(base::PartitionAllocNoHooks,
                                               alignment, size);
}

}  // namespace

namespace base {
namespace internal {

void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context) {
  return Allocator()->AllocFlagsNoHooks(0, MaybeAdjustSize(size),
                                        PartitionPageSize());
}

void* PartitionMallocUnchecked(const AllocatorDispatch*,
                               size_t size,
                               void* context) {
  return Allocator()->AllocFlagsNoHooks(base::PartitionAllocReturnNull,
                                        MaybeAdjustSize(size),
                                        PartitionPageSize());
}

void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context) {
  const size_t total = base::CheckMul(n, MaybeAdjustSize(size)).ValueOrDie();
  return Allocator()->AllocFlagsNoHooks(base::PartitionAllocZeroFill, total,
                                        PartitionPageSize());
}

void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context) {
  return AllocateAlignedMemory(alignment, size);
}

void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                            size_t size,
                            size_t alignment,
                            void* context) {
  return AllocateAlignedMemory(alignment, size);
}

// aligned_realloc documentation is
// https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/reference/aligned-realloc
// TODO(tasak): Expand the given memory block to the given size if possible.
// This realloc always free the original memory block and allocates a new memory
// block.
// TODO(tasak): Implement PartitionRoot<thread_safe>::AlignedReallocFlags and
// use it.
void* PartitionAlignedRealloc(const AllocatorDispatch* dispatch,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
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
#if defined(OS_APPLE)
  if (UNLIKELY(!base::IsManagedByPartitionAlloc(address) && address)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `realloc` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return realloc(address, size);
  }
#endif  // defined(OS_APPLE)

  return Allocator()->ReallocFlags(base::PartitionAllocNoHooks, address,
                                   MaybeAdjustSize(size), "");
}

void PartitionFree(const AllocatorDispatch*, void* address, void* context) {
#if defined(OS_APPLE)
  if (UNLIKELY(!base::IsManagedByPartitionAlloc(address) && address)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return free(address);
  }
#endif  // defined(OS_APPLE)

  base::ThreadSafePartitionRoot::FreeNoHooks(address);
}

size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context) {
  PA_DCHECK(address);

#if defined(OS_APPLE)
  if (!base::IsManagedByPartitionAlloc(address)) {
    // The object pointed to by `address` is not allocated by the
    // PartitionAlloc.  The return value `0` means that the pointer does not
    // belong to this malloc zone.
    return 0;
  }
#endif  // defined(OS_APPLE)

  // TODO(lizeb): Returns incorrect values for aligned allocations.
  const size_t size = base::ThreadSafePartitionRoot::GetUsableSize(address);
#if defined(OS_APPLE)
  // The object pointed to by `address` is allocated by the PartitionAlloc.
  // So, this function must not return zero so that the malloc zone dispatcher
  // finds the appropriate malloc zone.
  PA_DCHECK(size);
#endif  // defined(OS_APPLE)
  return size;
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
  PartitionAllocMemoryReclaimer::Instance()->RegisterPartition(Allocator());
  auto* original_root = OriginalAllocator();
  if (original_root)
    PartitionAllocMemoryReclaimer::Instance()->RegisterPartition(original_root);
  if (AlignedAllocator() != Allocator()) {
    PartitionAllocMemoryReclaimer::Instance()->RegisterPartition(
        AlignedAllocator());
  }
}

void ReconfigurePartitionAllocLazyCommit() {
  // Unlike other partitions, Allocator() and AlignedAllocator() do not
  // configure lazy commit upfront, because it uses base::Feature, which in turn
  // allocates memory. Thus, lazy commit configuration has to be done after
  // base::FeatureList is initialized.
  // TODO(bartekn): Aligned allocator can use the regular initialization path.
  Allocator()->ConfigureLazyCommit();
  auto* original_root = OriginalAllocator();
  if (original_root)
    original_root->ConfigureLazyCommit();
  AlignedAllocator()->ConfigureLazyCommit();
}

// Note that ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL implies that
// USE_BACKUP_REF_PTR is true.
#if BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)
alignas(base::ThreadSafePartitionRoot) uint8_t
    g_allocator_buffer_for_ref_count_config[sizeof(
        base::ThreadSafePartitionRoot)];

void ConfigurePartitionRefCountSupport(bool enable_ref_count) {
  auto* current_root = g_root.Get();
  current_root->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                            PartitionPurgeDiscardUnusedSystemPages);

  auto* new_root = new (g_allocator_buffer_for_ref_count_config)
      base::ThreadSafePartitionRoot({
#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
        base::PartitionOptions::AlignedAlloc::kDisallowed,
#else
        base::PartitionOptions::AlignedAlloc::kAllowed,
#endif
            base::PartitionOptions::ThreadCache::kEnabled,
            base::PartitionOptions::Quarantine::kAllowed,
#if BUILDFLAG(USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC)
            base::PartitionOptions::Cookies::kAllowed,
#else
            base::PartitionOptions::Cookies::kDisallowed,
#endif
            enable_ref_count ? base::PartitionOptions::RefCount::kAllowed
                             : base::PartitionOptions::RefCount::kDisallowed,
      });
  g_root.Replace(new_root);
  g_original_root_ = current_root;
}
#endif  // BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)

#if defined(PA_ALLOW_PCSCAN)
void EnablePCScan(bool dcscan) {
  internal::PCScan::Initialize(
      dcscan ? internal::PCScan::WantedWriteProtectionMode::kEnabled
             : internal::PCScan::WantedWriteProtectionMode::kDisabled);
  internal::PCScan::RegisterScannableRoot(Allocator());
  if (Allocator() != AlignedAllocator())
    internal::PCScan::RegisterScannableRoot(AlignedAllocator());
  internal::NonScannableAllocator::Instance().EnablePCScan();
}
#endif  // defined(PA_ALLOW_PCSCAN)

#if defined(OS_WIN)
// Call this as soon as possible during startup.
void ConfigurePartitionAlloc() {
#if defined(ARCH_CPU_X86)
  if (IsRunning32bitEmulatedOnArm64())
    g_extra_bytes = 8;
#endif  // defined(ARCH_CPU_X86)
}
#endif  // defined(OS_WIN)
}  // namespace allocator
}  // namespace base

constexpr AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &base::internal::PartitionMalloc,           // alloc_function
    &base::internal::PartitionMallocUnchecked,  // alloc_unchecked_function
    &base::internal::PartitionCalloc,    // alloc_zero_initialized_function
    &base::internal::PartitionMemalign,  // alloc_aligned_function
    &base::internal::PartitionRealloc,   // realloc_function
    &base::internal::PartitionFree,      // free_function
    &base::internal::PartitionGetSizeEstimate,  // get_size_estimate_function
    nullptr,                                    // batch_malloc_function
    nullptr,                                    // batch_free_function
    nullptr,                                    // free_definite_size_function
    &base::internal::PartitionAlignedAlloc,     // aligned_malloc_function
    &base::internal::PartitionAlignedRealloc,   // aligned_realloc_function
    &base::internal::PartitionFree,             // aligned_free_function
    nullptr,                                    // next
};

// Intercept diagnostics symbols as well, even though they are not part of the
// unified shim layer.
//
// TODO(lizeb): Implement the ones that doable.

extern "C" {

#if !defined(OS_APPLE)

SHIM_ALWAYS_EXPORT void malloc_stats(void) __THROW {}

SHIM_ALWAYS_EXPORT int mallopt(int cmd, int value) __THROW {
  return 0;
}

#endif  // !defined(OS_APPLE)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
  base::SimplePartitionStatsDumper allocator_dumper;
  Allocator()->DumpStats("malloc", true, &allocator_dumper);
  // TODO(bartekn): Dump OriginalAllocator() into "malloc" as well.

  base::SimplePartitionStatsDumper aligned_allocator_dumper;
  if (AlignedAllocator() != Allocator()) {
    AlignedAllocator()->DumpStats("posix_memalign", true,
                                  &aligned_allocator_dumper);
  }

  // Dump stats for nonscannable allocators.
  auto& nonscannable_allocator =
      base::internal::NonScannableAllocator::Instance();
  base::SimplePartitionStatsDumper nonscannable_allocator_dumper;
  if (auto* nonscannable_root = nonscannable_allocator.root())
    nonscannable_root->DumpStats("malloc", true,
                                 &nonscannable_allocator_dumper);

  struct mallinfo info = {0};
  info.arena = 0;  // Memory *not* allocated with mmap().

  // Memory allocated with mmap(), aka virtual size.
  info.hblks = allocator_dumper.stats().total_mmapped_bytes +
               aligned_allocator_dumper.stats().total_mmapped_bytes +
               nonscannable_allocator_dumper.stats().total_mmapped_bytes;
  // Resident bytes.
  info.hblkhd = allocator_dumper.stats().total_resident_bytes +
                aligned_allocator_dumper.stats().total_resident_bytes +
                nonscannable_allocator_dumper.stats().total_resident_bytes;
  // Allocated bytes.
  info.uordblks = allocator_dumper.stats().total_active_bytes +
                  aligned_allocator_dumper.stats().total_active_bytes +
                  nonscannable_allocator_dumper.stats().total_active_bytes;

  return info;
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // extern "C"

#if defined(OS_APPLE)

namespace base {
namespace allocator {

void InitializeDefaultAllocatorPartitionRoot() {
  // On OS_APPLE, the initialization of PartitionRoot uses memory allocations
  // internally, e.g. __builtin_available, and it's not easy to avoid it.
  // Thus, we initialize the PartitionRoot with using the system default
  // allocator before we intercept the system default allocator.
  ignore_result(Allocator());
}

}  // namespace allocator
}  // namespace base

#endif  // defined(OS_APPLE)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
