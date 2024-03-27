// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_CONFIG_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_CONFIG_H_

#include "build/build_config.h"
#include "partition_alloc/partition_alloc_base/debug/debugging_buildflags.h"
#include "partition_alloc/partition_alloc_buildflags.h"

// PA_CONFIG() uses a similar trick as BUILDFLAG() to allow the compiler catch
// typos or a missing #include.
//
// -----------------------------------------------------------------------------
// Housekeeping Rules
// -----------------------------------------------------------------------------
// 1. Prefix all config macros in this file with PA_CONFIG_ and define them in
//    a function-like manner, e.g. PA_CONFIG_MY_SETTING().
// 2. Both positive and negative cases must be defined.
// 3. Don't use PA_CONFIG_MY_SETTING() directly outside of its definition, use
//    PA_CONFIG(flag-without-PA_CONFIG_) instead, e.g. PA_CONFIG(MY_SETTING).
// 4. Do not use PA_CONFIG() when defining config macros, or it will lead to
//    recursion. Either use #if/#else, or PA_CONFIG_MY_SETTING() directly.
// 5. Similarly to above, but for a different reason, don't use defined() when
//    defining config macros. It'd violate -Wno-expansion-to-defined.
// 6. Try to use constexpr instead of macros wherever possible.
// TODO(bartekn): Convert macros to constexpr or BUILDFLAG as much as possible.
#define PA_CONFIG(flag) (PA_CONFIG_##flag())

// Assert that the heuristic in partition_alloc.gni is accurate on supported
// configurations.
#if BUILDFLAG(HAS_64_BIT_POINTERS)
static_assert(sizeof(void*) == 8, "");
#else
static_assert(sizeof(void*) != 8, "");
#endif  // PA_CONFIG(HAS_64_BITS_POINTERS)

#if BUILDFLAG(HAS_64_BIT_POINTERS) && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__)) && defined(__ARM_FP)
#define PA_CONFIG_STARSCAN_NEON_SUPPORTED() 1
#else
#define PA_CONFIG_STARSCAN_NEON_SUPPORTED() 0
#endif

#if BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(IS_IOS)
// Allow PA to select an alternate pool size at run-time before initialization,
// rather than using a single constexpr value.
//
// This is needed on iOS because iOS test processes can't handle large pools
// (see crbug.com/1250788).
//
// This setting is specific to 64-bit, as 32-bit has a different implementation.
#define PA_CONFIG_DYNAMICALLY_SELECT_POOL_SIZE() 1
#else
#define PA_CONFIG_DYNAMICALLY_SELECT_POOL_SIZE() 0
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(IS_IOS)

#if BUILDFLAG(HAS_64_BIT_POINTERS) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID))
#include <linux/version.h>
// TODO(bikineev): Enable for ChromeOS.
#define PA_CONFIG_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED() \
  (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0))
#else
#define PA_CONFIG_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED() 0
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS) &&
        // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID))

#if BUILDFLAG(USE_STARSCAN)
// Use card table to avoid races for PCScan configuration without safepoints.
// The card table provides the guaranteee that for a marked card the underling
// super-page is fully initialized.
#define PA_CONFIG_STARSCAN_USE_CARD_TABLE() 1
#else
// The card table is permanently disabled for 32-bit.
#define PA_CONFIG_STARSCAN_USE_CARD_TABLE() 0
#endif  // BUILDFLAG(USE_STARSCAN)

// Use batched freeing when sweeping pages. This builds up a freelist in the
// scanner thread and appends to the slot-span's freelist only once.
#define PA_CONFIG_STARSCAN_BATCHED_FREE() 1

// TODO(bikineev): Temporarily disable inlining in *Scan to get clearer
// stacktraces.
#define PA_CONFIG_STARSCAN_NOINLINE_SCAN_FUNCTIONS() 1

// TODO(bikineev): Temporarily disable *Scan in MemoryReclaimer as it seems to
// cause significant jank.
#define PA_CONFIG_STARSCAN_ENABLE_STARSCAN_ON_RECLAIM() 0

// Double free detection comes with expensive cmpxchg (with the loop around it).
// We currently disable it to improve the runtime.
#define PA_CONFIG_STARSCAN_EAGER_DOUBLE_FREE_DETECTION_ENABLED() 0

// POSIX is not only UNIX, e.g. macOS and other OSes. We do use Linux-specific
// features such as futex(2).
#define PA_CONFIG_HAS_LINUX_KERNEL() \
  (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))

// On some platforms, we implement locking by spinning in userspace, then going
// into the kernel only if there is contention. This requires platform support,
// namely:
// - On Linux, futex(2)
// - On Windows, a fast userspace "try" operation which is available with
//   SRWLock
// - On macOS, pthread_mutex_trylock() is fast by default starting with macOS
//   10.14. Chromium targets an earlier version, so it cannot be known at
//   compile-time. So we use something different.
//   TODO(https://crbug.com/1459032): macOS 10.15 is now required; switch to
//   better locking.
// - Otherwise, on POSIX we assume that a fast userspace pthread_mutex_trylock()
//   is available.
//
// Otherwise, a userspace spinlock implementation is used.
#if PA_CONFIG(HAS_LINUX_KERNEL) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#define PA_CONFIG_HAS_FAST_MUTEX() 1
#else
#define PA_CONFIG_HAS_FAST_MUTEX() 0
#endif

// If defined, enables zeroing memory on Free() with roughly 1% probability.
// This applies only to normal buckets, as direct-map allocations are always
// decommitted.
// TODO(bartekn): Re-enable once PartitionAlloc-Everywhere evaluation is done.
#define PA_CONFIG_ZERO_RANDOMLY_ON_FREE() 0

// Need TLS support.
#define PA_CONFIG_THREAD_CACHE_SUPPORTED() \
  (BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA))

// Too expensive for official builds, as it adds cache misses to all
// allocations. On the other hand, we want wide metrics coverage to get
// realistic profiles.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && !defined(OFFICIAL_BUILD)
#define PA_CONFIG_THREAD_CACHE_ALLOC_STATS() 1
#else
#define PA_CONFIG_THREAD_CACHE_ALLOC_STATS() 0
#endif

// Optional statistics collection. Lightweight, contrary to the ones above,
// hence enabled by default.
#define PA_CONFIG_THREAD_CACHE_ENABLE_STATISTICS() 1

// Enable free list shadow entry to strengthen hardening as much as possible.
// The shadow entry is an inversion (bitwise-NOT) of the encoded `next` pointer.
//
// Disabled on Big Endian CPUs, because encoding is also a bitwise-NOT there,
// making the shadow entry equal to the original, valid pointer to the next
// slot. In case Use-after-Free happens, we'd rather not hand out a valid,
// ready-to-use pointer.
#if defined(ARCH_CPU_LITTLE_ENDIAN)
#define PA_CONFIG_HAS_FREELIST_SHADOW_ENTRY() 1
#else
#define PA_CONFIG_HAS_FREELIST_SHADOW_ENTRY() 0
#endif

#if BUILDFLAG(HAS_MEMORY_TAGGING)
static_assert(sizeof(void*) == 8);
#endif

// Specifies whether allocation extras need to be added.
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#define PA_CONFIG_EXTRAS_REQUIRED() 1
#else
#define PA_CONFIG_EXTRAS_REQUIRED() 0
#endif

// Count and total wall clock time spent in memory related system calls. This
// doesn't cover all system calls, in particular the ones related to locking.
//
// Not enabled by default, as it has a runtime cost, and causes issues with some
// builds (e.g. Windows).
// However the total count is collected on all platforms.
#define PA_CONFIG_COUNT_SYSCALL_TIME() 0

// On Windows, |thread_local| variables cannot be marked "dllexport", see
// compiler error C2492 at
// https://docs.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/compiler-error-c2492?view=msvc-160.
// Don't use it there.
//
// On macOS and iOS:
// - With PartitionAlloc-Everywhere, thread_local allocates, reentering the
//   allocator.
// - Component builds triggered a clang bug: crbug.com/1243375
//
// On GNU/Linux and ChromeOS:
// - `thread_local` allocates, reentering the allocator.
//
// Regardless, the "normal" TLS access is fast on x86_64 (see partition_tls.h),
// so don't bother with thread_local anywhere.
#if !(BUILDFLAG(IS_WIN) && defined(COMPONENT_BUILD)) && \
    !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
#define PA_CONFIG_THREAD_LOCAL_TLS() 1
#else
#define PA_CONFIG_THREAD_LOCAL_TLS() 0
#endif

// When PartitionAlloc is malloc(), detect malloc() becoming re-entrant by
// calling malloc() again.
//
// Limitations:
// - BUILDFLAG(PA_DCHECK_IS_ON) due to runtime cost
// - thread_local TLS to simplify the implementation
// - Not on Android due to bot failures
#if BUILDFLAG(PA_DCHECK_IS_ON) && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_LOCAL_TLS) && !BUILDFLAG(IS_ANDROID)
#define PA_CONFIG_HAS_ALLOCATION_GUARD() 1
#else
#define PA_CONFIG_HAS_ALLOCATION_GUARD() 0
#endif

// On Android, we have to go through emutls, since this is always a shared
// library, so don't bother.
#if PA_CONFIG(THREAD_LOCAL_TLS) && !BUILDFLAG(IS_ANDROID)
#define PA_CONFIG_THREAD_CACHE_FAST_TLS() 1
#else
#define PA_CONFIG_THREAD_CACHE_FAST_TLS() 0
#endif

// Lazy commit should only be enabled on Windows, because commit charge is
// only meaningful and limited on Windows. It affects performance on other
// platforms and is simply not needed there due to OS supporting overcommit.
#if BUILDFLAG(IS_WIN)
constexpr bool kUseLazyCommit = true;
#else
constexpr bool kUseLazyCommit = false;
#endif

// On these platforms, lock all the partitions before fork(), and unlock after.
// This may be required on more platforms in the future.
#define PA_CONFIG_HAS_ATFORK_HANDLER() \
  (BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

// PartitionAlloc uses PartitionRootEnumerator to acquire all
// PartitionRoots at BeforeFork and to release at AfterFork.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_CONFIG(HAS_ATFORK_HANDLER)
#define PA_CONFIG_USE_PARTITION_ROOT_ENUMERATOR() 1
#else
#define PA_CONFIG_USE_PARTITION_ROOT_ENUMERATOR() 0
#endif

// Enable in-slot metadata cookie checks when dcheck_is_on or BRP slow checks
// are on. However, don't do this if that would cause InSlotMetadata to grow
// past the size that would fit in InSlotMetadataTable (see
// partition_alloc_constants.h), which currently can happen only when DPD is on.
#define PA_CONFIG_IN_SLOT_METADATA_CHECK_COOKIE() \
  (!(BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && \
     BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)) && \
   (BUILDFLAG(PA_DCHECK_IS_ON) ||                 \
    BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)))

// Use available space in the reference count to store the initially requested
// size from the application. This is used for debugging.
#if !PA_CONFIG(IN_SLOT_METADATA_CHECK_COOKIE) && \
    !BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
// Set to 1 when needed.
#define PA_CONFIG_IN_SLOT_METADATA_STORE_REQUESTED_SIZE() 0
#else
// You probably want it at 0, outside of local testing, or else
// PartitionRefCount will grow past 8B.
#define PA_CONFIG_IN_SLOT_METADATA_STORE_REQUESTED_SIZE() 0
#endif

#if PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE) && \
    PA_CONFIG(IN_SLOT_METADATA_CHECK_COOKIE)
#error "Cannot use a cookie *and* store the allocation size"
#endif

// Prefer smaller slot spans.
//
// Smaller slot spans may improve dirty memory fragmentation, but may also
// increase address space usage.
//
// This is intended to roll out more broadly, but only enabled on Linux for now
// to get performance bot and real-world data pre-A/B experiment.
//
// Also enabled on ARM64 macOS and iOS, as the 16kiB pages on this platform lead
// to larger slot spans.
#if BUILDFLAG(IS_LINUX) || (BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64))
#define PA_CONFIG_PREFER_SMALLER_SLOT_SPANS() 1
#else
#define PA_CONFIG_PREFER_SMALLER_SLOT_SPANS() 0
#endif

// Enable shadow metadata.
//
// With this flag, shadow pools will be mapped, on which writable shadow
// metadatas are placed, and the real metadatas are set to read-only instead.
// This feature is only enabled with 64-bit environment because pools work
// differently with 32-bits pointers (see glossary).
#if BUILDFLAG(ENABLE_SHADOW_METADATA_FOR_64_BITS_POINTERS) && \
    BUILDFLAG(HAS_64_BIT_POINTERS)
#define PA_CONFIG_ENABLE_SHADOW_METADATA() 1
#else
#define PA_CONFIG_ENABLE_SHADOW_METADATA() 0
#endif

// According to crbug.com/1349955#c24, macOS 11 has a bug where they assert that
// malloc_size() of an allocation is equal to the requested size. This is
// generally not true. The assert passed only because it happened to be true for
// the sizes they requested. BRP changes that, hence can't be deployed without a
// workaround.
//
// The bug has been fixed in macOS 12. Here we can only check the platform, and
// the version is checked dynamically later.
//
// The settings has MAYBE_ in the name, because the final decision to enable is
// based on the operarting system version check done at run-time.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && BUILDFLAG(IS_MAC)
#define PA_CONFIG_MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK() 1
#else
#define PA_CONFIG_MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK() 0
#endif

#if BUILDFLAG(ENABLE_POINTER_COMPRESSION)

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#error "Dynamically selected pool size is currently not supported"
#endif
#if BUILDFLAG(HAS_MEMORY_TAGGING)
// TODO(1376980): Address MTE once it's enabled.
#error "Compressed pointers don't support tag in the upper bits"
#endif

#endif  // BUILDFLAG(ENABLE_POINTER_COMPRESSION)

// PA_CONFIG(IS_NONCLANG_MSVC): mimics the compound condition used by
// Chromium's `//base/compiler_specific.h` to detect true (non-Clang)
// MSVC.
#if defined(COMPILER_MSVC) && !defined(__clang__)
#define PA_CONFIG_IS_NONCLANG_MSVC() 1
#else
#define PA_CONFIG_IS_NONCLANG_MSVC() 0
#endif

// Set GN build override 'assert_cpp_20' to false to disable assertion.
#if BUILDFLAG(ASSERT_CPP_20)
static_assert(__cplusplus >= 202002L,
              "PartitionAlloc targets C++20 or higher.");
#endif  // BUILDFLAG(ASSERT_CPP_20)

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_CONFIG_H_
