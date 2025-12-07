// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_YIELD_TO_LOOPER_CHECKER_H_
#define BASE_ANDROID_YIELD_TO_LOOPER_CHECKER_H_

#include "base/base_export.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"

namespace base::android {

// A class to track specific scenarios in which the UI message_pump should yield
// to the looper. Currently yields if an embedder's startup is running.
// Must be constructed on UI thread. All public methods must be called on the UI
// thread.
class BASE_EXPORT YieldToLooperChecker {
 public:
  static YieldToLooperChecker& GetInstance();

  // Update the checker on the startup status.
  void SetStartupRunning(bool is_startup_running);
  // Returns true if startup is running.
  bool ShouldYield();

 private:
  YieldToLooperChecker() = default;
  ~YieldToLooperChecker() = default;

  bool is_startup_running_ = false;
  THREAD_CHECKER(thread_checker_);

  friend class base::NoDestructor<YieldToLooperChecker>;
};

}  // namespace base::android

#endif  // BASE_ANDROID_YIELD_TO_LOOPER_CHECKER_H_
