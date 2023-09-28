// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_CONSUMER_SESSION_METRICS_DELEGATE_H_
#define ASH_AMBIENT_METRICS_AMBIENT_CONSUMER_SESSION_METRICS_DELEGATE_H_

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"
#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

// For all non-enterprise ambient sessions (the primary use case).
class ASH_EXPORT AmbientConsumerSessionMetricsDelegate
    : public AmbientSessionMetricsRecorder::Delegate {
 public:
  explicit AmbientConsumerSessionMetricsDelegate(AmbientUiSettings ui_settings);
  AmbientConsumerSessionMetricsDelegate(
      const AmbientConsumerSessionMetricsDelegate&) = delete;
  AmbientConsumerSessionMetricsDelegate& operator=(
      const AmbientConsumerSessionMetricsDelegate&) = delete;
  ~AmbientConsumerSessionMetricsDelegate() override;

  // AmbientSessionMetricsRecorder::Delegate:
  void RecordActivation() override;
  void RecordInitStatus(bool success) override;
  void RecordStartupTime(base::TimeDelta startup_time) override;
  void RecordEngagementTime(base::TimeDelta engagement_time) override;
  void RecordScreenCount(int num_screens) override;

 private:
  const AmbientUiSettings ui_settings_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_CONSUMER_SESSION_METRICS_DELEGATE_H_
