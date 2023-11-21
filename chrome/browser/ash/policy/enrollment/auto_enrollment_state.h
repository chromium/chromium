// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_

#include <string_view>
#include <variant>

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
// TODO(b/309921228): Remove once `AutoEnrollmentError` does not use legacy
// errors.
enum class AutoEnrollmentLegacyError {
  // Failed to connect to DMServer or to synchronize the system clock.
  kConnectionError,
  // Connection successful, but the server failed to generate a valid reply.
  kServerError,
};

// Represents a state determination error due to a timeout.
struct AutoEnrollmentSafeguardTimeoutError {
  constexpr bool operator==(const AutoEnrollmentSafeguardTimeoutError&) const =
      default;
};

// Represents a state determination error during clock sync.
struct AutoEnrollmentSystemClockSyncError {
  constexpr bool operator==(const AutoEnrollmentSystemClockSyncError&) const =
      default;
};

using AutoEnrollmentError = std::variant<AutoEnrollmentLegacyError,
                                         AutoEnrollmentSafeguardTimeoutError,
                                         AutoEnrollmentSystemClockSyncError>;

// Indicates the current state of the auto-enrollment check.
using AutoEnrollmentState =
    base::expected<AutoEnrollmentResult, AutoEnrollmentError>;

static constexpr AutoEnrollmentState kAutoEnrollmentLegacyConnectionError =
    base::unexpected(AutoEnrollmentLegacyError::kConnectionError);

static constexpr AutoEnrollmentState kAutoEnrollmentLegacyServerError =
    base::unexpected(AutoEnrollmentLegacyError::kServerError);

// Provides a way to report legacy errors and handle new errors as corresponding
// legacy ones.
// TODO(b/309921228): Remove once `AutoEnrollmentError` does not use legacy
// errors.
AutoEnrollmentLegacyError AutoEnrollmentErrorToLegacyError(
    const AutoEnrollmentError& error);

std::string_view AutoEnrollmentStateToString(const AutoEnrollmentState& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
