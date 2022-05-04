// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/metrics_recorder.h"

#include "ash/components/login/auth/auth_status_consumer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace ash {
namespace {

// Histogram for tracking the reason of auth failure
constexpr char kFailureReasonHistogramName[] = "Login.FailureReason";

// Histogram for tracking the reason of login success
constexpr char kSuccessReasonHistogramName[] = "Login.SuccessReason";

}  // namespace

MetricsRecorder::MetricsRecorder() {}

MetricsRecorder::~MetricsRecorder() = default;

void MetricsRecorder::OnAuthFailure(const AuthFailure::FailureReason& reason) {
  base::RecordAction(base::UserMetricsAction("Login_Failure"));
  UMA_HISTOGRAM_ENUMERATION(kFailureReasonHistogramName, reason,
                            AuthFailure::NUM_FAILURE_REASONS);
}

void MetricsRecorder::OnLoginSuccess(const SuccessReason& reason) {
  base::RecordAction(base::UserMetricsAction("Login_Success"));
  UMA_HISTOGRAM_ENUMERATION(kSuccessReasonHistogramName, reason,
                            SuccessReason::NUM_SUCCESS_REASONS);
}

void MetricsRecorder::OnGuestLoignSuccess() {
  base::RecordAction(base::UserMetricsAction("Login_GuestLoginSuccess"));
}

}  // namespace ash
