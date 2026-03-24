// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment_rust_shim.h"

namespace base::test {

std::unique_ptr<SingleThreadTaskEnvironment> CreateTaskEnvironment() {
  return std::make_unique<SingleThreadTaskEnvironment>();
}

}  // namespace base::test
