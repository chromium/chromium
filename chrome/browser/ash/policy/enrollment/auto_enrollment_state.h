// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_

#include <optional>
#include <string_view>
#include <variant>

#include "base/types/expected.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

struct DMServerJobResult;

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

// Represents an error during state determination request to DMServer. May
// be caused by connection error, server error, or invalid request.
struct AutoEnrollmentDMServerError {
  static AutoEnrollmentDMServerError FromDMServerJobResult(
      const DMServerJobResult& result);

  constexpr bool operator==(const AutoEnrollmentDMServerError&) const = default;

  DeviceManagementStatus dm_error;
  std::optional<int> network_error;
};

// Represents an error due to an invalid response from DMServer.
struct AutoEnrollmentStateAvailabilityResponseError {
  constexpr bool operator==(
      const AutoEnrollmentStateAvailabilityResponseError&) const = default;
};

// Represents an internal error in PSM library during initial state
// determination.
struct AutoEnrollmentPsmError {
  constexpr bool operator==(const AutoEnrollmentPsmError&) const = default;
};

// Represents an error due to an invalid response from DMServer.
struct AutoEnrollmentStateRetrievalResponseError {
  constexpr bool operator==(
      const AutoEnrollmentStateRetrievalResponseError&) const = default;
};

using AutoEnrollmentError =
    std::variant<AutoEnrollmentLegacyError,
                 AutoEnrollmentSafeguardTimeoutError,
                 AutoEnrollmentSystemClockSyncError,
                 AutoEnrollmentDMServerError,
                 AutoEnrollmentStateAvailabilityResponseError,
                 AutoEnrollmentPsmError,
                 AutoEnrollmentStateRetrievalResponseError>;

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

std::string AutoEnrollmentStateToString(const AutoEnrollmentState& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
