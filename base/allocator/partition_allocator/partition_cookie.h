// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_COOKIE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_COOKIE_H_

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"

namespace partition_alloc::internal {

static constexpr size_t kCookieSize = 16;

// Cookie is enabled for debug builds.
#if BUILDFLAG(PA_DCHECK_IS_ON)

inline constexpr unsigned char kCookieValue[kCookieSize] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xD0, 0x0D,
    0x13, 0x37, 0xF0, 0x05, 0xBA, 0x11, 0xAB, 0x1E};

constexpr size_t kPartitionCookieSizeAdjustment = kCookieSize;

PA_ALWAYS_INLINE void PartitionCookieCheckValue(unsigned char* cookie_ptr) {
  for (size_t i = 0; i < kCookieSize; ++i, ++cookie_ptr)
    PA_DCHECK(*cookie_ptr == kCookieValue[i]);
}

PA_ALWAYS_INLINE void PartitionCookieWriteValue(unsigned char* cookie_ptr) {
  for (size_t i = 0; i < kCookieSize; ++i, ++cookie_ptr)
    *cookie_ptr = kCookieValue[i];
}

#else

constexpr size_t kPartitionCookieSizeAdjustment = 0;

PA_ALWAYS_INLINE void PartitionCookieCheckValue(unsigned char* address) {}

PA_ALWAYS_INLINE void PartitionCookieWriteValue(unsigned char* cookie_ptr) {}

#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_COOKIE_H_
