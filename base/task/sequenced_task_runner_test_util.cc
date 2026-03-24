// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner_test_util.h"

#include "base/run_loop.h"

namespace base::task::test {

TestRefCounted::TestRefCounted(bool& destroyed_flag)
    : destroyed_flag_(destroyed_flag) {
  destroyed_flag_ = false;
}

TestRefCounted::~TestRefCounted() {
  destroyed_flag_ = true;
}

TestRefCounted* CreateTestRefCounted(bool& destroyed_flag) {
  return base::MakeRefCounted<TestRefCounted>(destroyed_flag).release();
}

}  // namespace base::task::test
