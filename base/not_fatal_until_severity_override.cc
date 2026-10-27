// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/not_fatal_until_severity_override.h"

namespace base::internal {

std::optional<NotFatalUntilSeverityOverride>&
GetNotFatalUntilSeverityOverride() {
  static std::optional<NotFatalUntilSeverityOverride> severity_override;
  return severity_override;
}

}  // namespace base::internal
