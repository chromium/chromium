// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RUN_LOOP_RUST_SHIM_H_
#define BASE_RUN_LOOP_RUST_SHIM_H_

#include "base/run_loop.h"

namespace base {

// Create a new RunLoop on the heap so we can pass it over the cxx bridge
std::unique_ptr<RunLoop> CreateRunLoop();

// Run the run loop.
void RunRunLoop(const std::unique_ptr<RunLoop>& run_loop);

// Quit the run loop.
void QuitRunLoop(const std::unique_ptr<RunLoop>& run_loop);

}  // namespace base

#endif  // BASE_RUN_LOOP_RUST_SHIM_H_
