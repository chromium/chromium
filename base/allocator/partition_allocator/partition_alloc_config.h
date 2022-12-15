// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_

#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "build/build_config.h"

// ARCH_CPU_64_BITS implies 64-bit instruction set, but not necessarily 64-bit
// address space. The only known case where address space is 32-bit is NaCl, so
// eliminate it explicitly. static_assert below ensures that others won't slip
// through.
#if defined(ARCH_CPU_64_BITS) && !BUILDFLAG(IS_NACL)
#define PA_HAS_64_BITS_POINTERS
static_assert(sizeof(void*) == 8, "");
#else
static_assert(sizeof(void*) != 8, "");
#endif  // defined(ARCH_CPU_64_BITS) && !BUILDFLAG(IS_NACL)

// PCScan supports 64 bits only and is disabled outside Chromium.
#if defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(STARSCAN)
#define PA_ALLOW_PCSCAN
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__)) && defined(__ARM_FP)
#define PA_STARSCAN_NEON_SUPPORTED
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN))
// Allow PA to select an alternate pool size at run-time before initialization,
// rather than using a single constexpr value.
//
// This is needed on iOS because iOS test processes can't handle large pools
// (see crbug.com/1250788).
//
// This is needed on Windows, because OS versions <8.1 incur commit charge even
// on reserved address space, thus don't handle large pools well (see
// crbug.com/1101421 and crbug.com/1217759).
//
// This setting is specific to 64-bit, as 32-bit has a different implementation.
#define PA_DYNAMICALLY_SELECT_POOL_SIZE
#endif  // defined(PA_HAS_64_BITS_POINTERS) &&
        // (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN))

// Puts the regular and BRP pools right next to each other, so that we can
// check "belongs to one of the two pools" with a single bitmask operation.
//
// This setting is specific to 64-bit, as 32-bit has a different implementation.
#if defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(GLUE_CORE_POOLS)
#define PA_GLUE_CORE_POOLS
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID))
#include <linux/version.h>
// TODO(bikineev): Enable for ChromeOS.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED
#endif
#endif  // defined(PA_HAS_64_BITS_POINTERS) &&
        // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID))

#if defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(STARSCAN)
// Use card table to avoid races for PCScan configuration without safepoints.
// The card table provides the guaranteee that for a marked card the underling
// super-page is fully initialized.
#define PA_STARSCAN_USE_CARD_TABLE 1
#else
// The card table is permanently disabled for 32-bit.
#define PA_STARSCAN_USE_CARD_TABLE 0
#endif  // defined(PA_HAS_64_BITS_POINTERS)

#if PA_STARSCAN_USE_CARD_TABLE && !defined(PA_ALLOW_PCSCAN)
#error "Card table can only be used when *Scan is allowed"
#endif

// Use batched freeing when sweeping pages. This builds up a freelist in the
// scanner thread and appends to the slot-span's freelist only once.
#define PA_STARSCAN_BATCHED_FREE 1

// POSIX is not only UNIX, e.g. macOS and other OSes. We do use Linux-specific
// features such as futex(2).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#define PA_HAS_LINUX_KERNEL
#endif

// On some platforms, we implement locking by spinning in userspace, then going
// into the kernel only if there is contention. This requires platform support,
// namely:
// - On Linux, futex(2)
// - On Windows, a fast userspace "try" operation which is available
//   with SRWLock
// - On macOS, pthread_mutex_trylock() is fast by default starting with macOS
//   10.14. Chromium targets an earlier version, so it cannot be known at
//   compile-time. So we use something different.
// - Otherwise, on POSIX we assume that a fast userspace pthread_mutex_trylock()
//   is available.
//
// Otherwise, a userspace spinlock implementation is used.
#if defined(PA_HAS_LINUX_KERNEL) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#define PA_HAS_FAST_MUTEX
#endif

// If defined, enables zeroing memory on Free() with roughly 1% probability.
// This applies only to normal buckets, as direct-map allocations are always
// decommitted.
// TODO(bartekn): Re-enable once PartitionAlloc-Everywhere evaluation is done.
#if 0
#define PA_ZERO_RANDOMLY_ON_FREE
#endif

// Need TLS support.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
#define PA_THREAD_CACHE_SUPPORTED
#endif

// Too expensive for official builds, as it adds cache misses to all
// allocations. On the other hand, we want wide metrics coverage to get
// realistic profiles.
#if BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT) && \
    !defined(OFFICIAL_BUILD)
#define PA_THREAD_CACHE_ALLOC_STATS
#endif

// Optional statistics collection. Lightweight, contrary to the ones above,
// hence enabled by default.
#define PA_THREAD_CACHE_ENABLE_STATISTICS

// Enable free list shadow entry to strengthen hardening as much as possible.
// The shadow entry is an inversion (bitwise-NOT) of the encoded `next` pointer.
//
// Disabled when ref-count is placed in the previous slot, as it will overlap
// with the shadow for the smallest slots.
//
// Disabled on Big Endian CPUs, because encoding is also a bitwise-NOT there,
// making the shadow entry equal to the original, valid pointer to the next
// slot. In case Use-after-Free happens, we'd rather not hand out a valid,
// ready-to-use pointer.
#if !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && \
    defined(ARCH_CPU_LITTLE_ENDIAN)
#define PA_HAS_FREELIST_SHADOW_ENTRY
#endif

#if defined(ARCH_CPU_ARM64) && defined(__clang__) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID))
static_assert(sizeof(void*) == 8);
#define PA_HAS_MEMORY_TAGGING
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
#define PA_USE_OOB_POISON
#endif

// Build MTECheckedPtr code.
//
// Only applicable to code with 64-bit pointers. Currently conflicts with true
// hardware MTE.
#if BUILDFLAG(ENABLE_MTE_CHECKED_PTR_SUPPORT) && \
    defined(PA_HAS_64_BITS_POINTERS) && !defined(PA_HAS_MEMORY_TAGGING)
static_assert(sizeof(void*) == 8);
#define PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS
#endif

// Specifies whether allocation extras need to be added.
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
#define PA_EXTRAS_REQUIRED
#endif

// Count and total wall clock time spent in memory related system calls. This
// doesn't cover all system calls, in particular the ones related to locking.
//
// Not enabled by default, as it has a runtime cost, and causes issues with some
// builds (e.g. Windows).
// However the total count is collected on all platforms.
// #define PA_COUNT_SYSCALL_TIME

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
// Regardless, the "normal" TLS access is fast on x86_64 (see partition_tls.h),
// so don't bother with thread_local anywhere.
#if !(BUILDFLAG(IS_WIN) && defined(COMPONENT_BUILD)) && !BUILDFLAG(IS_APPLE)
#define PA_THREAD_LOCAL_TLS
#endif

// When PartitionAlloc is malloc(), detect malloc() becoming re-entrant by
// calling malloc() again.
//
// Limitations:
// - BUILDFLAG(PA_DCHECK_IS_ON) due to runtime cost
// - thread_local TLS to simplify the implementation
// - Not on Android due to bot failures
#if BUILDFLAG(PA_DCHECK_IS_ON) &&                          \
    BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT) && \
    defined(PA_THREAD_LOCAL_TLS) && !BUILDFLAG(IS_ANDROID)
#define PA_HAS_ALLOCATION_GUARD
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
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define PA_HAS_ATFORK_HANDLER
#endif

// PartitionAlloc uses PartitionRootEnumerator to acquire all
// PartitionRoots at BeforeFork and to release at AfterFork.
#if BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT) && \
    defined(PA_HAS_ATFORK_HANDLER)
#define PA_USE_PARTITION_ROOT_ENUMERATOR
#endif

// Due to potential conflict with the free list pointer in the "previous slot"
// mode in the smallest bucket, we can't check both the cookie and the dangling
// raw_ptr at the same time.
#if !(BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) &&  \
      BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)) && \
    (BUILDFLAG(PA_DCHECK_IS_ON) ||                  \
     BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS))
#define PA_REF_COUNT_CHECK_COOKIE
#endif

// Use available space in the reference count to store the initially requested
// size from the application. This is used for debugging. On mac, it is used to
// workaround a bug. (crbug.com/1378822)
#if BUILDFLAG(IS_MAC) && !defined(PA_REF_COUNT_CHECK_COOKIE) && \
    !BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
#define PA_REF_COUNT_STORE_REQUESTED_SIZE
#endif

#if defined(PA_REF_COUNT_STORE_REQUESTED_SIZE) && \
    defined(PA_REF_COUNT_CHECK_COOKIE)
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
// Also enabled on ARM64 macOS, as the 16kiB pages on this platform lead to
// larger slot spans.
#if BUILDFLAG(IS_LINUX) || (BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64))
#define PA_PREFER_SMALLER_SLOT_SPANS
#endif

// Enable shadow metadata.
//
// With this flag, shadow pools will be mapped, on which writable shadow
// metadatas are placed, and the real metadatas are set to read-only instead.
// This feature is only enabled with 64-bit environment because pools work
// differently with 32-bits pointers (see glossary).
#if BUILDFLAG(ENABLE_SHADOW_METADATA_FOR_64_BITS_POINTERS) && \
    defined(PA_HAS_64_BITS_POINTERS)
#define PA_ENABLE_SHADOW_METADATA
#endif

// According to crbug.com/1349955#c24, macOS 11 has a bug where they asset that
// malloc_size() of an allocation is equal to the requested size. This is
// generally not true. The assert passed only because it happened to be true for
// the sizes they requested. BRP changes that, hence can't be deployed without a
// workaround.
//
// The bug has been fixed in macOS 12. Here we can only check the platform, and
// the version is checked dynamically later.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && BUILDFLAG(IS_MAC)
#define PA_ENABLE_MAC11_MALLOC_SIZE_HACK
#endif

// Enables compressed (4-byte) pointers that can point within the core pools
// (Regular + BRP).
#if defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(ENABLE_POINTER_COMPRESSION)

#define PA_POINTER_COMPRESSION

#if !defined(PA_GLUE_CORE_POOLS)
#error "Pointer compression works only with contiguous pools"
#endif
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
#error "Dynamically selected pool size is currently not supported"
#endif
#if defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS) || \
    defined(PA_HAS_MEMORY_TAGGING)
// TODO(1376980): Address MTE once it's enabled.
#error "Compressed pointers don't support tag in the upper bits"
#endif

#endif  // defined(PA_HAS_64_BITS_POINTERS) &&
        // BUILDFLAG(ENABLE_POINTER_COMPRESSION)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_
