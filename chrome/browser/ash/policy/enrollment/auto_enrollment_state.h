// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_

#include <string_view>

#include "base/types/expected.h"

namespace policy {

// Indicates the result of state determination.
enum class AutoEnrollmentResult {
  // Check completed successfully, enrollment should be triggered.
  kEnrollment,
  // Check completed successfully, enrollment not applicable.
  kNoEnrollment,
  // Check completed successfully, device is disabled.
  kDisabled,
};

// Indicates an error during state determination.
enum class AutoEnrollmentLegacyError {
  // Failed to connect to DMServer or to synchronize the system clock.
  kConnectionError,
  // Connection successful, but the server failed to generate a valid reply.
  kServerError,
};

// Indicates the current state of the auto-enrollment check.
using AutoEnrollmentState =
    base::expected<AutoEnrollmentResult, AutoEnrollmentLegacyError>;

static constexpr AutoEnrollmentState kAutoEnrollmentLegacyConnectionError =
    base::unexpected(AutoEnrollmentLegacyError::kConnectionError);

static constexpr AutoEnrollmentState kAutoEnrollmentLegacyServerError =
    base::unexpected(AutoEnrollmentLegacyError::kServerError);

std::string_view AutoEnrollmentStateToString(const AutoEnrollmentState& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
