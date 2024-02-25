// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"

#include "base/functional/overloaded.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
    case AutoEnrollmentResult::kSuggestedEnrollment:
      return "Suggested enrollment";
  }
}

std::string AutoEnrollmentErrorToString(AutoEnrollmentError error) {
  return absl::visit(
      base::Overloaded{
          [](AutoEnrollmentSafeguardTimeoutError) {
            return std::string("Safeguard timeout");
          },
          [](AutoEnrollmentSystemClockSyncError) {
            return std::string("System clock sync error");
          },
          [](AutoEnrollmentStateKeysRetrievalError) {
            return std::string("State keys retrieval error");
          },
          [](const AutoEnrollmentDMServerError& error) {
            return base::StringPrintf(
                "DMServer error: %d, %s", error.dm_error,
                net::ErrorToString(error.network_error.value_or(net::OK))
                    .c_str());
          },
          [](AutoEnrollmentStateAvailabilityResponseError error) {
            return std::string("Invalid state availability response");
          },
          [](AutoEnrollmentPsmError) {
            return std::string("PSM internal error");
          },
          [](AutoEnrollmentStateRetrievalResponseError) {
            return std::string("Invalid state retrieval response");
          },
      },
      error);
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

std::string AutoEnrollmentStateToString(const AutoEnrollmentState& state) {
  if (state.has_value()) {
    return std::string(AutoEnrollmentResultToString(state.value()));
  } else {
    return AutoEnrollmentErrorToString(state.error());
  }
}

}  // namespace policy
