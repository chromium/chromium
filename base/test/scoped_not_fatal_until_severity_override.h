// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_NOT_FATAL_UNTIL_SEVERITY_OVERRIDE_H_
#define BASE_TEST_SCOPED_NOT_FATAL_UNTIL_SEVERITY_OVERRIDE_H_

#include "base/not_fatal_until.h"

namespace base::internal {

// Overrides the severity of all `CHECK(..., milestone)` in the current scope.
// Forcing the severity to `logging::LOGGING_FATAL` allows tests to validate
// that the code will crash when `milestone` is reached. Forcing the severity to
// `logging::LOGGING_ERROR` allows tests to validate the non-fatal behavior that
// users will observe until that milestone is reached.
class ScopedNotFatalUntilSeverityOverride {
 public:
  ScopedNotFatalUntilSeverityOverride(base::NotFatalUntil milestone,
                                      bool is_fatal);
  ~ScopedNotFatalUntilSeverityOverride();
  ScopedNotFatalUntilSeverityOverride(
      const ScopedNotFatalUntilSeverityOverride&) = delete;
  ScopedNotFatalUntilSeverityOverride& operator=(
      const ScopedNotFatalUntilSeverityOverride&) = delete;

  // Returns true if `milestone` has already been reached and is therefore
  // unconditionally fatal in all build configurations.
  static bool IsMilestoneReached(base::NotFatalUntil milestone);
};

}  // namespace base::internal

#endif  // BASE_TEST_SCOPED_NOT_FATAL_UNTIL_SEVERITY_OVERRIDE_H_
