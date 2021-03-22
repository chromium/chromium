// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_TEST_UTIL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_TEST_UTIL_H_

#include <cstddef>

namespace base {
namespace internal {

bool IsLargeMemoryDevice();

// Only supported on POSIX systems, limits total data usage using
// setrlimit(RLIMIT_DATA).
bool SetDataLimit(size_t limit);
bool ClearDataLimit();

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_TEST_UTIL_H_
