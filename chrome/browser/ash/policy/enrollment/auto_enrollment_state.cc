// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"

#include "base/functional/overloaded.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

namespace policy {

namespace {

std::string_view AutoEnrollmentResultToString(AutoEnrollmentResult result) {
  switch (result) {
    case AutoEnrollmentResult::kEnrollment:
      return "Enrollment";
    case AutoEnrollmentResult::kNoEnrollment:
      return "No enrollment";
    case AutoEnrollmentResult::kDisabled:
      return "Device disabled";
  }
}

std::string_view AutoEnrollmentLegacyErrorCodeToString(
    AutoEnrollmentLegacyError error) {
  switch (error) {
    case policy::AutoEnrollmentLegacyError::kConnectionError:
      return "Connection error";
    case AutoEnrollmentLegacyError::kServerError:
      return "Server error";
  }
}

}  // namespace

// static
AutoEnrollmentDMServerError AutoEnrollmentDMServerError::FromDMServerJobResult(
    const DMServerJobResult& result) {
  CHECK_NE(result.dm_status, DM_STATUS_SUCCESS);

  return AutoEnrollmentDMServerError{
      .dm_error = result.dm_status,
      .network_error = (result.dm_status == DM_STATUS_REQUEST_FAILED
                            ? std::optional(result.net_error)
                            : std::nullopt)};
}

// static
AutoEnrollmentLegacyError AutoEnrollmentErrorToLegacyError(
    const AutoEnrollmentError& error) {
  return std::visit(
      base::Overloaded{
          [](AutoEnrollmentLegacyError legacy_error) { return legacy_error; },
          [](AutoEnrollmentSafeguardTimeoutError) {
            return AutoEnrollmentLegacyError::kConnectionError;
          },
          [](AutoEnrollmentSystemClockSyncError) {
            return AutoEnrollmentLegacyError::kConnectionError;
          },
          [](const AutoEnrollmentDMServerError& error) {
            return error.network_error.has_value()
                       ? AutoEnrollmentLegacyError::kConnectionError
                       : AutoEnrollmentLegacyError::kServerError;
          },
          [](AutoEnrollmentStateAvailabilityResponseError error) {
            return AutoEnrollmentLegacyError::kServerError;
          },
          [](AutoEnrollmentPsmError error) {
            return AutoEnrollmentLegacyError::kServerError;
          },
          [](AutoEnrollmentStateRetrievalResponseError) {
            return AutoEnrollmentLegacyError::kServerError;
          },
      },
      error);
}

std::string_view AutoEnrollmentStateToString(const AutoEnrollmentState& state) {
  if (state.has_value()) {
    return AutoEnrollmentResultToString(state.value());
  } else {
    return AutoEnrollmentLegacyErrorCodeToString(
        AutoEnrollmentErrorToLegacyError(state.error()));
  }
}

}  // namespace policy
