// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/shim/allocator_shim.h"

#include <errno.h>

#include <atomic>
#include <new>

#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/memory/page_size.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_WIN)
#include <unistd.h>
#else
#include "base/allocator/partition_allocator/shim/winheap_stubs_win.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>

#include "base/allocator/partition_allocator/partition_alloc_base/mac/mach_logging.h"
#include "base/allocator/partition_allocator/shim/allocator_interception_mac.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

// No calls to malloc / new in this file. They would would cause re-entrancy of
// the shim, which is hard to deal with. Keep this code as simple as possible
// and don't use any external C++ object here, not even //base ones. Even if
// they are safe to use today, in future they might be refactored.

namespace {

std::atomic<const allocator_shim::AllocatorDispatch*> g_chain_head{
    &allocator_shim::AllocatorDispatch::default_dispatch};

bool g_call_new_handler_on_malloc_failure = false;

PA_ALWAYS_INLINE size_t GetCachedPageSize() {
  static size_t pagesize = 0;
  if (!pagesize) {
    pagesize = partition_alloc::internal::base::GetPageSize();
  }
  return pagesize;
}

// Calls the std::new handler thread-safely. Returns true if a new_handler was
// set and called, false if no new_handler was set.
bool CallNewHandler(size_t size) {
#if BUILDFLAG(IS_WIN)
  return allocator_shim::WinCallNewHandler(size);
#else
  std::new_handler nh = std::get_new_handler();
  if (!nh) {
    return false;
  }
  (*nh)();
  // Assume the new_handler will abort if it fails. Exception are disabled and
  // we don't support the case of a new_handler throwing std::bad_balloc.
  return true;
#endif
}

PA_ALWAYS_INLINE const allocator_shim::AllocatorDispatch* GetChainHead() {
  return g_chain_head.load(std::memory_order_relaxed);
}

}  // namespace

namespace allocator_shim {

void SetCallNewHandlerOnMallocFailure(bool value) {
  g_call_new_handler_on_malloc_failure = value;

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  allocator_shim::internal::PartitionAllocSetCallNewHandlerOnMallocFailure(
      value);
#endif
}

void* UncheckedAlloc(size_t size) {
  const AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->alloc_unchecked_function(chain_head, size, nullptr);
}

void UncheckedFree(void* ptr) {
  const AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->free_function(chain_head, ptr, nullptr);
}

void InsertAllocatorDispatch(AllocatorDispatch* dispatch) {
  // Loop in case of (an unlikely) race on setting the list head.
  size_t kMaxRetries = 7;
  for (size_t i = 0; i < kMaxRetries; ++i) {
    const AllocatorDispatch* chain_head = GetChainHead();
    dispatch->next = chain_head;

    // This function guarantees to be thread-safe w.r.t. concurrent
    // insertions. It also has to guarantee that all the threads always
    // see a consistent chain, hence the atomic_thread_fence() below.
    // InsertAllocatorDispatch() is NOT a fastpath, as opposite to malloc(), so
    // we don't really want this to be a release-store with a corresponding
    // acquire-load during malloc().
    std::atomic_thread_fence(std::memory_order_seq_cst);
    // Set the chain head to the new dispatch atomically. If we lose the race,
    // retry.
    if (g_chain_head.compare_exchange_strong(chain_head, dispatch,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
      // Success.
      return;
    }
  }

  PA_CHECK(false);  // Too many retries, this shouldn't happen.
}

void RemoveAllocatorDispatchForTesting(AllocatorDispatch* dispatch) {
  PA_DCHECK(GetChainHead() == dispatch);
  g_chain_head.store(dispatch->next, std::memory_order_relaxed);
}

#if BUILDFLAG(IS_APPLE)
void TryFreeDefaultFallbackToFindZoneAndFree(void* ptr) {
  unsigned int zone_count = 0;
  vm_address_t* zones = nullptr;
  kern_return_t result =
      malloc_get_all_zones(mach_task_self(), nullptr, &zones, &zone_count);
  PA_MACH_CHECK(result == KERN_SUCCESS, result) << "malloc_get_all_zones";

  // "find_zone_and_free" expected by try_free_default.
  //
  // libmalloc's zones call find_registered_zone() in case the default one
  // doesn't handle the allocation. We can't, so we try to emulate it. See the
  // implementation in libmalloc/src/malloc.c for details.
  // https://github.com/apple-oss-distributions/libmalloc/blob/main/src/malloc.c
  for (unsigned int i = 0; i < zone_count; ++i) {
    malloc_zone_t* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
    if (size_t size = zone->size(zone, ptr)) {
      if (zone->version >= 6 && zone->free_definite_size) {
        zone->free_definite_size(zone, ptr, size);
      } else {
        zone->free(zone, ptr);
      }
      return;
    }
  }

  // There must be an owner zone.
  PA_CHECK(false);
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace allocator_shim

// The Shim* functions below are the entry-points into the shim-layer and
// are supposed to be invoked by the allocator_shim_override_*
// headers to route the malloc / new symbols through the shim layer.
// They are defined as ALWAYS_INLINE in order to remove a level of indirection
// between the system-defined entry points and the shim implementations.
extern "C" {

// The general pattern for allocations is:
// - Try to allocate, if succeded return the pointer.
// - If the allocation failed:
//   - Call the std::new_handler if it was a C++ allocation.
//   - Call the std::new_handler if it was a malloc() (or calloc() or similar)
//     AND SetCallNewHandlerOnMallocFailure(true).
//   - If the std::new_handler is NOT set just return nullptr.
//   - If the std::new_handler is set:
//     - Assume it will abort() if it fails (very likely the new_handler will
//       just suicide printing a message).
//     - Assume it did succeed if it returns, in which case reattempt the alloc.

PA_ALWAYS_INLINE void* ShimCppNew(size_t size) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr;
  do {
    void* context = nullptr;
#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    context = malloc_default_zone();
#endif
    ptr = chain_head->alloc_function(chain_head, size, context);
  } while (!ptr && CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimCppNewNoThrow(size_t size) {
  void* context = nullptr;
#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  context = malloc_default_zone();
#endif
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->alloc_unchecked_function(chain_head, size, context);
}

PA_ALWAYS_INLINE void* ShimCppAlignedNew(size_t size, size_t alignment) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr;
  do {
    void* context = nullptr;
#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    context = malloc_default_zone();
#endif
    ptr = chain_head->alloc_aligned_function(chain_head, alignment, size,
                                             context);
  } while (!ptr && CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void ShimCppDelete(void* address) {
  void* context = nullptr;
#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  context = malloc_default_zone();
#endif
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->free_function(chain_head, address, context);
}

PA_ALWAYS_INLINE void* ShimMalloc(size_t size, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->alloc_function(chain_head, size, context);
  } while (!ptr && g_call_new_handler_on_malloc_failure &&
           CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimCalloc(size_t n, size_t size, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->alloc_zero_initialized_function(chain_head, n, size,
                                                      context);
  } while (!ptr && g_call_new_handler_on_malloc_failure &&
           CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimRealloc(void* address, size_t size, void* context) {
  // realloc(size == 0) means free() and might return a nullptr. We should
  // not call the std::new_handler in that case, though.
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->realloc_function(chain_head, address, size, context);
  } while (!ptr && size && g_call_new_handler_on_malloc_failure &&
           CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimMemalign(size_t alignment,
                                    size_t size,
                                    void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr;
  do {
    ptr = chain_head->alloc_aligned_function(chain_head, alignment, size,
                                             context);
  } while (!ptr && g_call_new_handler_on_malloc_failure &&
           CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE int ShimPosixMemalign(void** res,
                                       size_t alignment,
                                       size_t size) {
  // posix_memalign is supposed to check the arguments. See tc_posix_memalign()
  // in tc_malloc.cc.
  if (((alignment % sizeof(void*)) != 0) ||
      !partition_alloc::internal::base::bits::IsPowerOfTwo(alignment)) {
    return EINVAL;
  }
  void* ptr = ShimMemalign(alignment, size, nullptr);
  *res = ptr;
  return ptr ? 0 : ENOMEM;
}

PA_ALWAYS_INLINE void* ShimValloc(size_t size, void* context) {
  return ShimMemalign(GetCachedPageSize(), size, context);
}

PA_ALWAYS_INLINE void* ShimPvalloc(size_t size) {
  // pvalloc(0) should allocate one page, according to its man page.
  if (size == 0) {
    size = GetCachedPageSize();
  } else {
    size = partition_alloc::internal::base::bits::AlignUp(size,
                                                          GetCachedPageSize());
  }
  // The third argument is nullptr because pvalloc is glibc only and does not
  // exist on OSX/BSD systems.
  return ShimMemalign(GetCachedPageSize(), size, nullptr);
}

PA_ALWAYS_INLINE void ShimFree(void* address, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->free_function(chain_head, address, context);
}

PA_ALWAYS_INLINE size_t ShimGetSizeEstimate(const void* address,
                                            void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->get_size_estimate_function(
      chain_head, const_cast<void*>(address), context);
}

PA_ALWAYS_INLINE bool ShimClaimedAddress(void* address, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->claimed_address_function(chain_head, address, context);
}

PA_ALWAYS_INLINE unsigned ShimBatchMalloc(size_t size,
                                          void** results,
                                          unsigned num_requested,
                                          void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->batch_malloc_function(chain_head, size, results,
                                           num_requested, context);
}

PA_ALWAYS_INLINE void ShimBatchFree(void** to_be_freed,
                                    unsigned num_to_be_freed,
                                    void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->batch_free_function(chain_head, to_be_freed,
                                         num_to_be_freed, context);
}

PA_ALWAYS_INLINE void ShimFreeDefiniteSize(void* ptr,
                                           size_t size,
                                           void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->free_definite_size_function(chain_head, ptr, size,
                                                 context);
}

PA_ALWAYS_INLINE void ShimTryFreeDefault(void* ptr, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->try_free_default_function(chain_head, ptr, context);
}

PA_ALWAYS_INLINE void* ShimAlignedMalloc(size_t size,
                                         size_t alignment,
                                         void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr = nullptr;
  do {
    ptr = chain_head->aligned_malloc_function(chain_head, size, alignment,
                                              context);
  } while (!ptr && g_call_new_handler_on_malloc_failure &&
           CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void* ShimAlignedRealloc(void* address,
                                          size_t size,
                                          size_t alignment,
                                          void* context) {
  // _aligned_realloc(size == 0) means _aligned_free() and might return a
  // nullptr. We should not call the std::new_handler in that case, though.
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  void* ptr = nullptr;
  do {
    ptr = chain_head->aligned_realloc_function(chain_head, address, size,
                                               alignment, context);
  } while (!ptr && size && g_call_new_handler_on_malloc_failure &&
           CallNewHandler(size));
  return ptr;
}

PA_ALWAYS_INLINE void ShimAlignedFree(void* address, void* context) {
  const allocator_shim::AllocatorDispatch* const chain_head = GetChainHead();
  return chain_head->aligned_free_function(chain_head, address, context);
}

}  // extern "C"

#if !BUILDFLAG(IS_WIN) && \
    !(BUILDFLAG(IS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC))
// Cpp symbols (new / delete) should always be routed through the shim layer
// except on Windows and macOS (except for PartitionAlloc-Everywhere) where the
// malloc intercept is deep enough that it also catches the cpp calls.
//
// In case of PartitionAlloc-Everywhere on macOS, malloc backed by
// allocator_shim::internal::PartitionMalloc crashes on OOM, and we need to
// avoid crashes in case of operator new() noexcept.  Thus, operator new()
// noexcept needs to be routed to
// allocator_shim::internal::PartitionMallocUnchecked through the shim layer.
#include "base/allocator/partition_allocator/shim/allocator_shim_override_cpp_symbols.h"
#endif

#if BUILDFLAG(IS_ANDROID)
// Android does not support symbol interposition. The way malloc symbols are
// intercepted on Android is by using link-time -wrap flags.
#include "base/allocator/partition_allocator/shim/allocator_shim_override_linker_wrapped_symbols.h"
#elif BUILDFLAG(IS_WIN)
// On Windows we use plain link-time overriding of the CRT symbols.
#include "base/allocator/partition_allocator/shim/allocator_shim_override_ucrt_symbols_win.h"
#elif BUILDFLAG(IS_APPLE)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/shim/allocator_shim_override_mac_default_zone.h"
#else  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/shim/allocator_shim_override_mac_symbols.h"
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#else
#include "base/allocator/partition_allocator/shim/allocator_shim_override_libc_symbols.h"
#endif

// Some glibc versions (until commit 6c444ad6e953dbdf9c7be065308a0a777)
// incorrectly call __libc_memalign() to allocate memory (see elf/dl-tls.c in
// glibc 2.23 for instance), and free() to free it. This causes issues for us,
// as we are then asked to free memory we didn't allocate.
//
// This only happened in glibc to allocate TLS storage metadata, and there are
// no other callers of __libc_memalign() there as of September 2020. To work
// around this issue, intercept this internal libc symbol to make sure that both
// the allocation and the free() are caught by the shim.
//
// This seems fragile, and is, but there is ample precedent for it, making it
// quite likely to keep working in the future. For instance, LLVM for LSAN uses
// this mechanism.

#if defined(LIBC_GLIBC) && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/shim/allocator_shim_override_glibc_weak_symbols.h"
#endif

#if BUILDFLAG(IS_APPLE)
namespace allocator_shim {

void InitializeAllocatorShim() {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Prepares the default dispatch. After the intercepted malloc calls have
  // traversed the shim this will route them to the default malloc zone.
  InitializeDefaultDispatchToMacAllocator();

  MallocZoneFunctions functions = MallocZoneFunctionsToReplaceDefault();

  // This replaces the default malloc zone, causing calls to malloc & friends
  // from the codebase to be routed to ShimMalloc() above.
  ReplaceFunctionsForStoredZones(&functions);
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace allocator_shim
#endif

// Cross-checks.

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#error The allocator shim should not be compiled when building for memory tools.
#endif

#if (defined(__GNUC__) && defined(__EXCEPTIONS)) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND))
#error This code cannot be used when exceptions are turned on.
#endif
