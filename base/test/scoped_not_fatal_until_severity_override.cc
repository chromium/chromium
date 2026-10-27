// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_not_fatal_until_severity_override.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_version_internal.h"
#include "base/logging/log_severity.h"
#include "base/not_fatal_until.h"
#include "base/not_fatal_until_severity_override.h"

namespace base::internal {

ScopedNotFatalUntilSeverityOverride::ScopedNotFatalUntilSeverityOverride(
    base::NotFatalUntil milestone,
    bool is_fatal) {
  CHECK(milestone != base::NotFatalUntil::NoSpecifiedMilestoneInternal);
  std::optional<NotFatalUntilSeverityOverride>& severity_override =
      GetNotFatalUntilSeverityOverride();
  CHECK(!severity_override);
  severity_override.emplace(
      milestone, is_fatal ? logging::LOGGING_FATAL : logging::LOGGING_ERROR);
}

ScopedNotFatalUntilSeverityOverride::~ScopedNotFatalUntilSeverityOverride() {
  std::optional<NotFatalUntilSeverityOverride>& severity_override =
      GetNotFatalUntilSeverityOverride();
  severity_override.reset();
}

// static
bool ScopedNotFatalUntilSeverityOverride::IsMilestoneReached(
    base::NotFatalUntil milestone) {
  CHECK(milestone != base::NotFatalUntil::NoSpecifiedMilestoneInternal);
  return std::to_underlying(milestone) <= BASE_CHECK_VERSION_INTERNAL;
}

}  // namespace base::internal
