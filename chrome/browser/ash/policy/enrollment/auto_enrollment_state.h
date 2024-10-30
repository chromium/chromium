// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_

#include <optional>
#include <string_view>

#include "base/types/expected.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
  // Check completed successfully, enrollment is suggested but not enforced.
  kSuggestedEnrollment,
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

// Represents an error while retrieving state keys.
struct AutoEnrollmentStateKeysRetrievalError {
  constexpr bool operator==(
      const AutoEnrollmentStateKeysRetrievalError&) const = default;
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
    absl::variant<AutoEnrollmentSafeguardTimeoutError,
                  AutoEnrollmentSystemClockSyncError,
                  AutoEnrollmentStateKeysRetrievalError,
                  AutoEnrollmentDMServerError,
                  AutoEnrollmentStateAvailabilityResponseError,
                  AutoEnrollmentPsmError,
                  AutoEnrollmentStateRetrievalResponseError>;

// Indicates the current state of the auto-enrollment check.
using AutoEnrollmentState =
    base::expected<AutoEnrollmentResult, AutoEnrollmentError>;

std::string AutoEnrollmentStateToString(const AutoEnrollmentState& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
