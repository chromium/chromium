// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_USER_METRICS_RECORDER_TEST_API_H_
#define ASH_METRICS_USER_METRICS_RECORDER_TEST_API_H_

#include "ash/metrics/user_metrics_recorder.h"

namespace ash {

// Test API to access internals of the UserMetricsRecorder class.
class UserMetricsRecorderTestAPI {
 public:
  UserMetricsRecorderTestAPI();

  UserMetricsRecorderTestAPI(const UserMetricsRecorderTestAPI&) = delete;
  UserMetricsRecorderTestAPI& operator=(const UserMetricsRecorderTestAPI&) =
      delete;

  ~UserMetricsRecorderTestAPI();

  // Accessor to UserMetricsRecorder::RecordPeriodicMetrics().
  void RecordPeriodicMetrics();

  // Accessor to UserMetricsRecorder::IsUserInActiveDesktopEnvironment().
  bool IsUserInActiveDesktopEnvironment() const;

 private:
  // The UserMetricsRecorder that |this| is providing internal access to.
  UserMetricsRecorder user_metrics_recorder_;
};

}  // namespace ash

#endif  // ASH_METRICS_USER_METRICS_RECORDER_TEST_API_H_
