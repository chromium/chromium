// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/yield_to_looper_checker.h"

namespace base::android {

void YieldToLooperChecker::SetStartupRunning(bool is_startup_running) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_startup_running_ = is_startup_running;
}

bool YieldToLooperChecker::ShouldYield() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return is_startup_running_;
}

// static
YieldToLooperChecker& YieldToLooperChecker::GetInstance() {
  static NoDestructor<YieldToLooperChecker> checker;
  return *checker.get();
}

}  // namespace base::android
