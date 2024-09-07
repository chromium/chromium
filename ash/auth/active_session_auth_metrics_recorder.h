// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_ACTIVE_SESSION_AUTH_METRICS_RECORDER_H_
#define ASH_AUTH_ACTIVE_SESSION_AUTH_METRICS_RECORDER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_common.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"

namespace ash {

// A metrics recorder that records pointer related metrics.
class ASH_EXPORT ActiveSessionAuthMetricsRecorder {
 public:
  ActiveSessionAuthMetricsRecorder();

  ActiveSessionAuthMetricsRecorder(const ActiveSessionAuthMetricsRecorder&) =
      delete;
  ActiveSessionAuthMetricsRecorder& operator=(
      const ActiveSessionAuthMetricsRecorder&) = delete;

  ~ActiveSessionAuthMetricsRecorder();

  void RecordShow(AuthRequest::Reason reason);
  void RecordClose();

  void RecordAuthStarted(AuthInputType input_type);
  void RecordAuthFailed(AuthInputType input_type);
  void RecordAuthSucceeded(AuthInputType input_type);

 private:
  std::optional<AuthRequest::Reason> open_reason_;
  std::optional<AuthInputType> started_auth_type_;
  std::optional<base::ElapsedTimer> open_timer_;

  int pin_attempt_counter_ = 0;
  int password_attempt_counter_ = 0;
  int fingerprint_attempt_counter_ = 0;
  bool auth_succeeded_ = false;
};

}  // namespace ash

#endif  // ASH_AUTH_ACTIVE_SESSION_AUTH_METRICS_RECORDER_H_
