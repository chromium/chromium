// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_METRICS_RECORDER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_METRICS_RECORDER_H_

#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_metrics_recorder.h"

namespace ash::eche_app {

class EcheConnectionMetricsRecorder
    : public secure_channel::NearbyMetricsRecorder {
 public:
  EcheConnectionMetricsRecorder();
  ~EcheConnectionMetricsRecorder() override;

  EcheConnectionMetricsRecorder(const EcheConnectionMetricsRecorder&) = delete;
  EcheConnectionMetricsRecorder& operator=(
      const EcheConnectionMetricsRecorder&) = delete;

  // secure_channel::NearbyMetricsRecorder:
  void RecordConnectionResult(bool success) override;
  void RecordConnectionFailureReason(
      secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
  void RecordConnectionLatency(const base::TimeDelta latency) override;
  void RecordConnectionDuration(const base::TimeDelta duration) override;
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_METRICS_RECORDER_H_
