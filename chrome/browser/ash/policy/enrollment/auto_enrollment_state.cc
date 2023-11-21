// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"

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

std::string_view AutoEnrollmentStateToString(const AutoEnrollmentState& state) {
  if (state.has_value()) {
    return AutoEnrollmentResultToString(state.value());
  } else {
    return AutoEnrollmentLegacyErrorCodeToString(state.error());
  }
}

}  // namespace policy
