// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_NONSCANNABLE_MEMORY_H_
#define BASE_MEMORY_NONSCANNABLE_MEMORY_H_

#include <cstddef>

#include "base/base_export.h"

// This file contains allocation/deallocation functions for memory that doesn't
// need to be scanned by PCScan. Such memory should only contain "data" objects,
// i.e. objects that don't have pointers/references to other objects. An example
// would be strings or socket/IPC/file buffers. Use with caution.
namespace base {

// Allocate/free non-scannable, but still quarantinable memory.
BASE_EXPORT void* AllocNonScannable(size_t size);
BASE_EXPORT void FreeNonScannable(void* ptr);

// Allocate/free non-scannable and non-quarantinable memory. These functions
// behave as normal, *Scan-unaware allocation functions. This can be useful for
// allocations that are guaranteed to be safe by the user, i.e. allocations that
// cannot be referenced from outside and cannot contain dangling references
// themselves.
BASE_EXPORT void* AllocNonQuarantinable(size_t size);
BASE_EXPORT void FreeNonQuarantinable(void* ptr);

// Deleters to be used with std::unique_ptr.
struct NonScannableDeleter {
  void operator()(void* ptr) const { FreeNonScannable(ptr); }
};
struct NonQuarantinableDeleter {
  void operator()(void* ptr) const { FreeNonQuarantinable(ptr); }
};

}  // namespace base

#endif  // BASE_MEMORY_NONSCANNABLE_MEMORY_H_
