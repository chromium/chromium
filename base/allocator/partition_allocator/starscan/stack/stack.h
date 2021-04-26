// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STACK_STACK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STACK_STACK_H_

#include <cstdint>

#include "base/base_export.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {

// Returns the current stack pointer.
// TODO(bikineev,1202644): Remove this once base/stack_util.h lands.
BASE_EXPORT NOINLINE uintptr_t* GetStackPointer();
// Returns the top of the stack using system API.
BASE_EXPORT void* GetStackTop();

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STACK_STACK_H_
