// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/active_session_auth_metrics_recorder.h"

#include <optional>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_common.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/timer/elapsed_timer.h"

namespace ash {

namespace {

// Constants for histograms.
constexpr char kShowReasonHistogram[] = "Ash.Auth.ActiveSessionShowReason";
constexpr char kAuthStartedHistogram[] = "Ash.Auth.ActiveSessionAuthStart";
constexpr char kAuthFailedHistogram[] = "Ash.Auth.ActiveSessionAuthFailed";
constexpr char kAuthSucceededHistogram[] =
    "Ash.Auth.ActiveSessionAuthSucceeded";
constexpr char kClosedWithSuccessHistogram[] =
    "Ash.Auth.ActiveSessionAuthClosedWithSuccess";
constexpr char kClosedDuringAuthHistogram[] =
    "Ash.Auth.ActiveSessionAuthClosedDuringAuth";
constexpr char kOpenDurationHistogram[] =
    "Ash.Auth.ActiveSessionAuthOpenDuration";
constexpr char kNumberOfPinAttemptHistogram[] =
    "Ash.Auth.ActiveSessionAuthPinAttempt";
constexpr char kNumberOfPasswordAttemptHistogram[] =
    "Ash.Auth.ActiveSessionAuthPasswordAttempt";

// The ceiling to use when clamping the number of PIN attempts that can be
// recorded for UMA collection.
constexpr int kMaxRecordedPinAttempts = 20;

// The ceiling to use when clamping the number of Password attempts that can be
// recorded for UMA collection.
constexpr int kMaxRecordedPasswordAttempts = 20;
}  // namespace

ActiveSessionAuthMetricsRecorder::ActiveSessionAuthMetricsRecorder() = default;

ActiveSessionAuthMetricsRecorder::~ActiveSessionAuthMetricsRecorder() = default;

void ActiveSessionAuthMetricsRecorder::RecordShow(
    ActiveSessionAuthController::Reason reason) {
  CHECK(!open_reason_.has_value());
  CHECK(!open_timer_.has_value());

  // Record to metric the reason when the ActiveSessionAuthWidget is shown.
  base::UmaHistogramEnumeration(kShowReasonHistogram, reason);

  open_reason_ = reason;
  open_timer_.emplace(base::ElapsedTimer());
}

void ActiveSessionAuthMetricsRecorder::RecordClose() {
  CHECK(open_reason_.has_value());
  CHECK(open_timer_.has_value());

  // Record to metric the dialog was closed after authentication succeeded or
  // not.
  base::UmaHistogramBoolean(kClosedWithSuccessHistogram, auth_succeeded_);

  // Record to metric the dialog was closed during authentication or not.
  base::UmaHistogramBoolean(kClosedDuringAuthHistogram,
                            started_auth_type_.has_value());

  // Record to metric how long was the dialog opened.
  base::UmaHistogramMediumTimes(kOpenDurationHistogram, open_timer_->Elapsed());

  // Record to metric the number of pin attempts.
  base::UmaHistogramExactLinear(kNumberOfPinAttemptHistogram,
                                pin_attempt_counter_, kMaxRecordedPinAttempts);

  // Record to metric the number of password attempts.
  base::UmaHistogramExactLinear(kNumberOfPasswordAttemptHistogram,
                                password_attempt_counter_,
                                kMaxRecordedPasswordAttempts);

  // Reset the state.
  auth_succeeded_ = false;
  pin_attempt_counter_ = 0;
  password_attempt_counter_ = 0;
  started_auth_type_.reset();
  open_reason_.reset();
  open_timer_.reset();
}

void ActiveSessionAuthMetricsRecorder::RecordAuthStarted(
    AuthInputType input_type) {
  CHECK(!started_auth_type_.has_value());
  switch (input_type) {
    case AuthInputType::kPassword:
      ++password_attempt_counter_;
      break;
    case AuthInputType::kPin:
      ++pin_attempt_counter_;
      break;
    default:
      NOTREACHED();
  }
  started_auth_type_ = input_type;

  // Record to metric the auth input type when an authentication started.
  base::UmaHistogramEnumeration(kAuthStartedHistogram, input_type);
}

void ActiveSessionAuthMetricsRecorder::RecordAuthFailed(
    AuthInputType input_type) {
  CHECK(started_auth_type_.has_value());
  CHECK_EQ(started_auth_type_.value(), input_type);

  // Record to metric the failed authentication type.
  base::UmaHistogramEnumeration(kAuthFailedHistogram, input_type);

  started_auth_type_.reset();
}

void ActiveSessionAuthMetricsRecorder::RecordAuthSucceeded(
    AuthInputType input_type) {
  CHECK(started_auth_type_.has_value());
  CHECK_EQ(started_auth_type_.value(), input_type);

  // Record to metric the succeeded authentication type.
  base::UmaHistogramEnumeration(kAuthSucceededHistogram, input_type);

  started_auth_type_.reset();
  auth_succeeded_ = true;
}

}  // namespace ash
