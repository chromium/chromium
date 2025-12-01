// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_WIN_WINDOWS_HANDLE_UTIL_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_WIN_WINDOWS_HANDLE_UTIL_H_

#include "partition_alloc/partition_alloc_base/win/windows_types.h"

namespace partition_alloc::internal::base {

inline bool IsPseudoHandle(HANDLE h) {
  // Note that there appears to be no official documentation covering the
  // existence of specific pseudo handle values. In practice it's clear that
  // e.g. -1 is the current process, -2 is the current thread, etc. The largest
  // negative value known to be an issue with DuplicateHandle in fuzzers is -12.
  //
  // Note that there is virtually no risk of a real handle value falling within
  // this range and being misclassified as a pseudo handle.
  //
  // Cast through uintptr_t and then signed int to make the truncation to 32
  // bits explicit. Handles are size of-pointer but are always 32-bit values.
  // https://msdn.microsoft.com/en-us/library/aa384203(VS.85).aspx says: 64-bit
  // versions of Windows use 32-bit handles for interoperability.
  static constexpr int kMinimumKnownPseudoHandleValue = -12;
  const auto value = static_cast<int32_t>(reinterpret_cast<uintptr_t>(h));
  return value < 0 && value >= kMinimumKnownPseudoHandleValue;
}

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_WIN_WINDOWS_HANDLE_UTIL_H_
