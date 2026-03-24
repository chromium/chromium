// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop_rust_shim.h"

namespace base {

std::unique_ptr<RunLoop> CreateRunLoop() {
  return std::make_unique<RunLoop>();
}

void RunRunLoop(const std::unique_ptr<RunLoop>& run_loop) {
  run_loop->Run();
}

void QuitRunLoop(const std::unique_ptr<RunLoop>& run_loop) {
  run_loop->Quit();
}

}  // namespace base
