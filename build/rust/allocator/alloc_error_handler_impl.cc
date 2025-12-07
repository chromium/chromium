// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/allocator/alloc_error_handler_impl.h"

#include "build/rust/allocator/alias.h"
#include "build/rust/allocator/immediate_crash.h"

namespace rust_allocator_internal {

void alloc_error_handler_impl() {
  NO_CODE_FOLDING();
  IMMEDIATE_CRASH();
}

}  // namespace rust_allocator_internal
