// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/user_metrics_recorder_test_api.h"

namespace ash {

UserMetricsRecorderTestAPI::UserMetricsRecorderTestAPI()
    : user_metrics_recorder_(false) {}

UserMetricsRecorderTestAPI::~UserMetricsRecorderTestAPI() = default;

void UserMetricsRecorderTestAPI::RecordPeriodicMetrics() {
  user_metrics_recorder_.RecordPeriodicMetrics();
}

bool UserMetricsRecorderTestAPI::IsUserInActiveDesktopEnvironment() const {
  return user_metrics_recorder_.IsUserInActiveDesktopEnvironment();
}

}  // namespace ash
