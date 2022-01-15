// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_

#include "base/allocator/buildflags.h"
#include "base/dcheck_is_on.h"
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
#endif

// PCScan supports 64 bits only.
#if defined(PA_HAS_64_BITS_POINTERS)
#define PA_ALLOW_PCSCAN
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__)) && defined(__ARM_FP)
#define PA_STARSCAN_NEON_SUPPORTED
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID))
#include <linux/version.h>
// TODO(bikineev): Enable for ChromeOS.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED
#endif
#endif

#if defined(PA_HAS_64_BITS_POINTERS)
// Use card table to avoid races for PCScan configuration without safepoints.
// The card table provides the guaranteee that for a marked card the underling
// super-page is fully initialized.
#define PA_STARSCAN_USE_CARD_TABLE 1
#else
// The card table is permanently disabled for 32-bit.
#define PA_STARSCAN_USE_CARD_TABLE 0
#endif

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
// - Otherwise, a fast userspace pthread_mutex_trylock().
//
// On macOS, pthread_mutex_trylock() is fast by default starting with macOS
// 10.14. Chromium targets an earlier version, so it cannot be known at
// compile-time. So we use something different. On other POSIX systems, we
// assume that pthread_mutex_trylock() is suitable.
//
// Otherwise, a userspace spinlock implementation is used.
#if defined(PA_HAS_LINUX_KERNEL) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)) || BUILDFLAG(IS_FUCHSIA)
#define PA_HAS_FAST_MUTEX
#endif

// If set to 1, enables zeroing memory on Free() with roughly 1% probability.
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
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && !defined(OFFICIAL_BUILD)
#define PA_THREAD_CACHE_ALLOC_STATS
#endif

// Optional statistics collection. Lightweight, contrary to the ones above,
// hence enabled by default.
#define PA_THREAD_CACHE_ENABLE_STATISTICS

// Enable free list hardening as much as possible.
//
// Disabled when putting refcount in the previous slot, which is what
// PUT_REF_COUNT_IN_PREVIOUS_SLOT does. In this case the refcount overlaps with
// the next pointer shadow for the smallest bucket.
//
// Only for Little endian CPUs, as the freelist encoding used on big endian
// platforms complicates things. Note that Chromium is not officially supported
// on any big endian architecture as well.
#if !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && \
    defined(ARCH_CPU_LITTLE_ENDIAN)
#define PA_HAS_FREELIST_HARDENING
#endif

// Specifies whether allocation extras need to be added.
#if DCHECK_IS_ON() || BUILDFLAG(USE_BACKUP_REF_PTR)
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
// - DCHECK_IS_ON() due to runtime cost
// - thread_local TLS to simplify the implementation
// - Not on Android due to bot failures
#if DCHECK_IS_ON() && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
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

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_
