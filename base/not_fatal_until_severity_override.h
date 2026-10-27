// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOT_FATAL_UNTIL_SEVERITY_OVERRIDE_H_
#define BASE_NOT_FATAL_UNTIL_SEVERITY_OVERRIDE_H_

#include <optional>

#include "base/base_export.h"
#include "base/logging/log_severity.h"
#include "base/not_fatal_until.h"

namespace base::internal {

// Stores the milestone severity override to use in `CHECK(..., NotFatalUntil)`.
struct NotFatalUntilSeverityOverride {
  base::NotFatalUntil milestone;
  logging::LogSeverity severity;
};

// Returns the global state used by
// `base::internal::ScopedNotFatalUntilSeverityOverride` to override the
// severity of `CHECK(..., NotFatalUntil)` in unit tests.
BASE_EXPORT std::optional<NotFatalUntilSeverityOverride>&
GetNotFatalUntilSeverityOverride();

}  // namespace base::internal

#endif  // BASE_NOT_FATAL_UNTIL_SEVERITY_OVERRIDE_H_
