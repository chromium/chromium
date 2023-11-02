// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H_
#define BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H_

#include <stddef.h>  // for size_t

#include "base/base_export.h"
#include "build/build_config.h"

namespace base {
namespace allocator {

// Request that the allocator release any free memory it knows about to the
// system.
BASE_EXPORT void ReleaseFreeMemory();

}  // namespace allocator
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H_
