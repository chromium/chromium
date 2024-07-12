// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_NONSCANNABLE_MEMORY_H_
#define BASE_MEMORY_NONSCANNABLE_MEMORY_H_

#include <cstddef>

#include "base/base_export.h"

// TODO(https://crbug.com/351126352): Remove this header.
// This file contains utility functions for PCScan algorithm.
// As PCScan being removed from the repository, all functions here
// just forward requests to the default allocator.
// Do not introduce new use of these functions.

namespace base {

BASE_EXPORT void* AllocNonScannable(size_t size);
BASE_EXPORT void FreeNonScannable(void* ptr);
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
