// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef BASE_ALLOCATOR_ALLOCATOR_CHECK_H_
#define BASE_ALLOCATOR_ALLOCATOR_CHECK_H_

#include "base/base_export.h"

namespace base {
namespace allocator {

BASE_EXPORT bool IsAllocatorInitialized();

}  // namespace allocator
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_CHECK_H_
