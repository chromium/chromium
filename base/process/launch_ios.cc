// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

namespace base {

void CheckPThreadStackMinIsSafe() {
  static_assert(__builtin_constant_p(PTHREAD_STACK_MIN),
                "Always constant on iOS");
}

void RaiseProcessToHighPriority() {
  // Impossible on iOS. Do nothing.
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  return false;
}

bool GetAppOutputAndError(const CommandLine& cl, std::string* output) {
  return false;
}

Process LaunchProcess(const CommandLine& cmdline,
                      const LaunchOptions& options) {
  return Process();
}

}  // namespace base
