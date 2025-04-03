// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include "build/rust/std/alias.h"
#include "build/rust/std/immediate_crash.h"

extern "C" {

// As part of rustc's contract for using `#[global_allocator]` without
// rustc-generated shims we must define this symbol, since we are opting in to
// unstable functionality. See https://github.com/rust-lang/rust/issues/123015
//
// Mark it weak since rustc will generate it when it drives linking.
[[maybe_unused]]
__attribute__((weak)) unsigned char __rust_no_alloc_shim_is_unstable;

__attribute__((weak)) void __rust_alloc_error_handler(size_t size,
                                                      size_t align) {
  NO_CODE_FOLDING();
  IMMEDIATE_CRASH();
}

__attribute__((
    weak)) extern const unsigned char __rust_alloc_error_handler_should_panic =
    0;

}  // extern "C"
