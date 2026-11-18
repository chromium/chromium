// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCED_RUST_FFI_H_
#define BASE_TASK_SEQUENCED_RUST_FFI_H_

#include <stdint.h>

#include "base/base_export.h"

namespace base {

// Returns an index for the current thread that's never 0, and that's never
// reused within the process, even after the thread exits. Rust's `ThreadLock`
// (//base/task/thread_lock.rs) uses it to record which thread holds the lock.
//
// This lives in the `base` component rather than in the Rust crate: a component
// build links a copy of a Rust library into every component that uses it, but
// loads `base` only once per process.
BASE_EXPORT uint32_t GetThreadIndexForRust();

}  // namespace base

#endif  // BASE_TASK_SEQUENCED_RUST_FFI_H_
