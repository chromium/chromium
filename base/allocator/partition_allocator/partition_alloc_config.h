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
#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)
#define PA_HAS_64_BITS_POINTERS
static_assert(sizeof(void*) == 8, "");
#else
static_assert(sizeof(void*) != 8, "");
#endif

// BackupRefPtr and PCScan are incompatible, and due to its conservative nature,
// it is 64 bits only.
// Disable PCScan even for USE_BACKUP_REF_PTR_FAKE, so that a "fake" BRP
// experiment is unaffected by PCScan, as a non-fake one would.
#if defined(PA_HAS_64_BITS_POINTERS) && !BUILDFLAG(USE_BACKUP_REF_PTR) && \
    !BUILDFLAG(USE_BACKUP_REF_PTR_FAKE)
#define PA_ALLOW_PCSCAN
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__)) && defined(__ARM_FP)
#define PA_STARSCAN_NEON_SUPPORTED
#endif

#if defined(PA_HAS_64_BITS_POINTERS) && \
    (defined(OS_LINUX) || defined(OS_ANDROID))
#include <linux/version.h>
// TODO(bikineev): Enable for ChromeOS.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED
#endif

#if defined(PA_HAS_64_BITS_POINTERS)
// Disable currently the card table to check the memory improvement.
#define PA_STARSCAN_USE_CARD_TABLE 0
#else
// The card table is permanently disabled for 32-bit.
#define PA_STARSCAN_USE_CARD_TABLE 0
#endif
#endif

#if PA_STARSCAN_USE_CARD_TABLE && !defined(PA_ALLOW_PCSCAN)
#error "Card table can only be used when *Scan is allowed"
#endif

// POSIX is not only UNIX, e.g. macOS and other OSes. We do use Linux-specific
// features such as futex(2).
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#define PA_HAS_LINUX_KERNEL
#endif

// SpinningMutex uses either futex(2) on Linux, or a fast userspace "try"
// operation, which is available on Windows.
#if defined(PA_HAS_LINUX_KERNEL) || defined(OS_WIN)
#define PA_HAS_SPINNING_MUTEX
#endif

// If set to 1, enables zeroing memory on Free() with roughly 1% probability.
// This applies only to normal buckets, as direct-map allocations are always
// decommitted.
// TODO(bartekn): Re-enable once PartitionAlloc-Everywhere evaluation is done.
#if 0
#define PA_ZERO_RANDOMLY_ON_FREE
#endif

// Need TLS support.
#if defined(OS_POSIX) || defined(OS_WIN)
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
// Disabled on ARM64 Macs, as this crashes very early (crbug.com/1172236).
// TODO(lizeb): Enable in as many configurations as possible.
//
// Disabled when putting refcount in the previous slot, which is what
// PUT_REF_COUNT_IN_PREVIOUS_SLOT does. In this case the refcount overlaps with
// the next pointer shadow for the smallest bucket.
#if !(defined(OS_MAC) && defined(ARCH_CPU_ARM64)) && \
    !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
#define PA_HAS_FREELIST_HARDENING
#endif

// Specifies whether allocation extras need to be added.
#if DCHECK_IS_ON() || BUILDFLAG(USE_BACKUP_REF_PTR)
#define PA_EXTRAS_REQUIRED
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONFIG_H_
