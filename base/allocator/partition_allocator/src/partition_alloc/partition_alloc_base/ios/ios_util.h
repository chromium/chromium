// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_IOS_IOS_UTIL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_IOS_IOS_UTIL_H_

#include <stdint.h>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc::internal::base::ios {

// Returns whether the operating system is iOS 12 or later.
// TODO(crbug.com/1129482): Remove once minimum supported version is at least 12
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool IsRunningOnIOS12OrLater();

// Returns whether the operating system is iOS 13 or later.
// TODO(crbug.com/1129483): Remove once minimum supported version is at least 13
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool IsRunningOnIOS13OrLater();

// Returns whether the operating system is iOS 14 or later.
// TODO(crbug.com/1129484): Remove once minimum supported version is at least 14
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool IsRunningOnIOS14OrLater();

// Returns whether the operating system is iOS 15 or later.
// TODO(crbug.com/1227419): Remove once minimum supported version is at least 15
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool IsRunningOnIOS15OrLater();

// Returns whether the operating system is at the given version or later.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool IsRunningOnOrLater(int32_t major, int32_t minor, int32_t bug_fix);

}  // namespace partition_alloc::internal::base::ios

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_IOS_IOS_UTIL_H_
