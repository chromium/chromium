// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"

#include "base/allocator/allocator_shim_internals.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/bits.h"
#include "base/no_destructor.h"
#include "build/build_config.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <malloc.h>
#endif

using base::allocator::AllocatorDispatch;

namespace {

// We would usually make g_root a static local variable, as these are guaranteed
// to be thread-safe in C++11. However this does not work on Windows, as the
// initialization calls into the runtime, which is not prepared to handle it.
//
// To sidestep that, we implement our own equivalent to a local `static
// base::NoDestructor<base::ThreadSafePartitionRoot> root`.
//
// The ingredients are:
// - Placement new to avoid a static constructor, and a static destructor.
// - Double-checked locking to get the same guarantees as a static local
//   variable.

// Lock for double-checked locking.
std::atomic<bool> g_initialization_lock;
std::atomic<base::ThreadSafePartitionRoot*> g_root_;
// Buffer for placement new.
alignas(base::ThreadSafePartitionRoot) uint8_t
    g_allocator_buffer[sizeof(base::ThreadSafePartitionRoot)];

base::ThreadSafePartitionRoot* Allocator() {
  // Double-checked locking.
  //
  // The proper way to proceed is:
  //
  // auto* root = load_acquire(g_root);
  // if (!root) {
  //   ScopedLock initialization_lock;
  //   root = load_relaxed(g_root);
  //   if (root)
  //     return root;
  //   new_root = Create new root.
  //   release_store(g_root, new_root);
  // }
  //
  // We don't want to use a base::Lock here, so instead we use the
  // compare-and-exchange on a lock variable, but this provides the same
  // guarantees as a regular lock. The code could be made simpler as we have
  // stricter requirements, but we stick to something close to a regular lock
  // for ease of reading, as none of this is performance-critical anyway.
  //
  // If we boldly assume that initialization will always be single-threaded,
  // then we could remove all these atomic operations, but this seems a bit too
  // bold to try yet. Might be worth revisiting though, since this would remove
  // a memory barrier at each load. We could probably guarantee single-threaded
  // init by adding a static constructor which allocates (and hence triggers
  // initialization before any other thread is created).
  auto* root = g_root_.load(std::memory_order_acquire);
  if (LIKELY(root))
    return root;

  bool expected = false;
  // Semantically equivalent to base::Lock::Acquire().
  while (!g_initialization_lock.compare_exchange_strong(
      expected, true, std::memory_order_acquire, std::memory_order_acquire)) {
    expected = false;
  }

  root = g_root_.load(std::memory_order_relaxed);
  // Someone beat us.
  if (root) {
    // Semantically equivalent to base::Lock::Release().
    g_initialization_lock.store(false, std::memory_order_release);
    return root;
  }

  auto* new_root = new (g_allocator_buffer) base::ThreadSafePartitionRoot({
    base::PartitionOptions::Alignment::kRegular,
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        base::PartitionOptions::ThreadCache::kEnabled,
#else
        // Other tests, such as the ThreadCache tests create a thread cache, and
        // only one is supported at a time.
        base::PartitionOptions::ThreadCache::kDisabled,
#endif
        base::PartitionOptions::PCScan::kDisabledByDefault
  });
  g_root_.store(new_root, std::memory_order_release);

  // Semantically equivalent to base::Lock::Release().
  g_initialization_lock.store(false, std::memory_order_release);
  return new_root;
}

base::ThreadSafePartitionRoot* AlignedAllocator() {
  // Since the general-purpose allocator uses the thread cache, this one cannot.
  static base::NoDestructor<base::ThreadSafePartitionRoot> aligned_allocator(
      base::PartitionOptions{
          base::PartitionOptions::Alignment::kAlignedAlloc,
          base::PartitionOptions::ThreadCache::kDisabled,
          base::PartitionOptions::PCScan::kDisabledByDefault});
  return aligned_allocator.get();
}

}  // namespace

namespace base {
namespace internal {

void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context) {
  return Allocator()->AllocFlagsNoHooks(0, size);
}

void* PartitionMallocUnchecked(const AllocatorDispatch*,
                               size_t size,
                               void* context) {
  return Allocator()->AllocFlagsNoHooks(base::PartitionAllocReturnNull, size);
}

void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context) {
  return Allocator()->AllocFlagsNoHooks(base::PartitionAllocZeroFill, n * size);
}

void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context) {
  return AlignedAllocator()->AlignedAllocFlags(base::PartitionAllocNoHooks,
                                               alignment, size);
}

void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                            size_t size,
                            size_t alignment,
                            void* context) {
  return AlignedAllocator()->AlignedAllocFlags(base::PartitionAllocNoHooks,
                                               alignment, size);
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
    new_ptr = AlignedAllocator()->AlignedAllocFlags(base::PartitionAllocNoHooks,
                                                    alignment, size);
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
  return Allocator()->ReallocFlags(base::PartitionAllocNoHooks, address, size,
                                   "");
}

void PartitionFree(const AllocatorDispatch*, void* address, void* context) {
  base::ThreadSafePartitionRoot::FreeNoHooks(address);
}

size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context) {
  // TODO(lizeb): Returns incorrect values for aligned allocations.
  return base::ThreadSafePartitionRoot::GetUsableSize(address);
}

// static
ThreadSafePartitionRoot* PartitionAllocMalloc::Allocator() {
  return ::Allocator();
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
  // Allocator() and AlignedAllocator() do not register their PartitionRoots to
  // the memory reclaimer because the memory reclaimer allocates memory.  Thus,
  // the registration to the memory reclaimer should be done sometime later.
  // This function will be called sometime appropriate after PartitionRoots are
  // initialized.
  PartitionAllocMemoryReclaimer::Instance()->RegisterPartition(Allocator());
  PartitionAllocMemoryReclaimer::Instance()->RegisterPartition(
      AlignedAllocator());
}

void EnablePCScanIfNeeded() {
  if (!features::IsPartitionAllocPCScanEnabled())
    return;
  Allocator()->EnablePCScan();
  AlignedAllocator()->EnablePCScan();
}

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

  base::SimplePartitionStatsDumper aligned_allocator_dumper;
  AlignedAllocator()->DumpStats("posix_memalign", true,
                                &aligned_allocator_dumper);

  struct mallinfo info = {0};
  info.arena = 0;  // Memory *not* allocated with mmap().

  // Memory allocated with mmap(), aka virtual size.
  info.hblks = allocator_dumper.stats().total_mmapped_bytes +
               aligned_allocator_dumper.stats().total_mmapped_bytes;
  // Resident bytes.
  info.hblkhd = allocator_dumper.stats().total_resident_bytes +
                aligned_allocator_dumper.stats().total_resident_bytes;
  // Allocated bytes.
  info.uordblks = allocator_dumper.stats().total_active_bytes +
                  aligned_allocator_dumper.stats().total_active_bytes;

  return info;
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // extern "C"

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
