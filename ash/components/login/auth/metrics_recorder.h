// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_METRICS_RECORDER_H_
#define ASH_COMPONENTS_LOGIN_AUTH_METRICS_RECORDER_H_

#include "ash/components/login/auth/auth_status_consumer.h"

namespace ash {

// This class encapsulates metrics reporting. User actions and behaviors are
// reported in multiple stages of the login flow. This metrics reporter would
// centralize the tracking and reporting.
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) MetricsRecorder {
 public:
  // Reports various metrics during the login flow.
  MetricsRecorder();
  MetricsRecorder(const MetricsRecorder&) = delete;
  MetricsRecorder& operator=(const MetricsRecorder&) = delete;
  MetricsRecorder(MetricsRecorder&&) = delete;
  MetricsRecorder& operator=(MetricsRecorder&&) = delete;
  ~MetricsRecorder();

  // Logs the auth failure action and reason.
  void OnAuthFailure(const AuthFailure::FailureReason& failure_reason);

  // Logs the login success action and reason.
  void OnLoginSuccess(const SuccessReason& reason);

  // Logs the guest login success action.
  void OnGuestLoignSuccess();
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_METRICS_RECORDER_H_