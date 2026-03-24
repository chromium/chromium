// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCED_TASK_RUNNER_RUST_SHIM_H_
#define BASE_TASK_SEQUENCED_TASK_RUNNER_RUST_SHIM_H_

#include "third_party/rust/cxx/v1/cxx.h"

namespace base {

class SequencedTaskRunner;
struct RustOnceClosure;

// Return a raw pointer to the current default SequencedTaskRunner, since we
// can't pass scoped_refptr to rust directly.
SequencedTaskRunner* GetCurrentDefaultSequencedTaskRunnerForRust();

// Post a task to the given runner
bool PostTaskFromRust(SequencedTaskRunner& runner,
                      rust::Box<base::RustOnceClosure> task);

// Expose ref-counting to Rust
void AddRef(const SequencedTaskRunner& runner);
void Release(const SequencedTaskRunner& runner);

}  // namespace base

#endif  // BASE_TASK_SEQUENCED_TASK_RUNNER_RUST_SHIM_H_
