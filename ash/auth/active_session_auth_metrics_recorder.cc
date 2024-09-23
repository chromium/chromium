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
constexpr char kNumberOfFingerprintAttemptHistogram[] =
    "Ash.Auth.ActiveSessionAuthFingerprintAttempt";

// The ceiling to use when clamping the number of PIN attempts that can be
// recorded for UMA collection.
constexpr int kMaxRecordedPinAttempts = 20;

// The ceiling to use when clamping the number of Password attempts that can be
// recorded for UMA collection.
constexpr int kMaxRecordedPasswordAttempts = 20;

// The ceiling to use when clamping the number of Fingerprint attempts that can
// be recorded for UMA collection.
constexpr int kMaxRecordedFingerprintAttempts = 20;

}  // namespace

ActiveSessionAuthMetricsRecorder::ActiveSessionAuthMetricsRecorder() = default;

ActiveSessionAuthMetricsRecorder::~ActiveSessionAuthMetricsRecorder() = default;

void ActiveSessionAuthMetricsRecorder::RecordShow(AuthRequest::Reason reason) {
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

  // Record to metric the number of fingerprint attempts.
  base::UmaHistogramExactLinear(kNumberOfFingerprintAttemptHistogram,
                                fingerprint_attempt_counter_,
                                kMaxRecordedFingerprintAttempts);

  // Reset the state.
  auth_succeeded_ = false;
  pin_attempt_counter_ = 0;
  password_attempt_counter_ = 0;
  fingerprint_attempt_counter_ = 0;
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
    case AuthInputType::kFingerprint:
      // Fingerprint authentication begins when the authentication dialog is
      // shown and the user has fingerprint record(s) and the policy doesn't
      // prevent its use. The fingerprint session remains active until
      // explicitly terminated, so even after a failed scan attempt, there's no
      // need to restart it. This session runs concurrently with password/PIN
      // authentication attempts. Therefore, we are not tracking "Authentication
      // Started" events for fingerprint specifically, but rather focusing on
      // the count of successful and failed attempts.
      NOTREACHED();
    default:
      NOTREACHED();
  }
  started_auth_type_ = input_type;

  // Record to metric the auth input type when an authentication started.
  base::UmaHistogramEnumeration(kAuthStartedHistogram, input_type);
}

void ActiveSessionAuthMetricsRecorder::RecordAuthFailed(
    AuthInputType input_type) {
  // Fingerprint authentication can occur concurrently with other
  // AuthInputType's.
  if (input_type == AuthInputType::kFingerprint) {
    ++fingerprint_attempt_counter_;
  } else {
    CHECK(started_auth_type_.has_value());
    CHECK_EQ(started_auth_type_.value(), input_type);

    started_auth_type_.reset();
  }

  // Record to metric the failed authentication type.
  base::UmaHistogramEnumeration(kAuthFailedHistogram, input_type);
}

void ActiveSessionAuthMetricsRecorder::RecordAuthSucceeded(
    AuthInputType input_type) {
  // Fingerprint authentication can occur concurrently with other
  // AuthInputType's.
  if (input_type == AuthInputType::kFingerprint) {
    ++fingerprint_attempt_counter_;
  } else {
    CHECK(started_auth_type_.has_value());
    CHECK_EQ(started_auth_type_.value(), input_type);
  }

  // Record to metric the succeeded authentication type.
  base::UmaHistogramEnumeration(kAuthSucceededHistogram, input_type);

  started_auth_type_.reset();
  auth_succeeded_ = true;
}

}  // namespace ash
