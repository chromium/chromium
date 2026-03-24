// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TASK_ENVIRONMENT_RUST_SHIM_H_
#define BASE_TEST_TASK_ENVIRONMENT_RUST_SHIM_H_

#include "base/test/task_environment.h"

namespace base::test {

// Create a task environment for testing.
std::unique_ptr<SingleThreadTaskEnvironment> CreateTaskEnvironment();

}  // namespace base::test

#endif  // BASE_TEST_TASK_ENVIRONMENT_RUST_SHIM_H_
