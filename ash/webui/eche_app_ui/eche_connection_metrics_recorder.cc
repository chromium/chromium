// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_metrics_recorder.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash::eche_app {

EcheConnectionMetricsRecorder::EcheConnectionMetricsRecorder() = default;
EcheConnectionMetricsRecorder::~EcheConnectionMetricsRecorder() = default;

void EcheConnectionMetricsRecorder::RecordConnectionResult(bool success) {
  base::UmaHistogramBoolean("Eche.Connection.Result", success);
}

void EcheConnectionMetricsRecorder::RecordConnectionFailureReason(
    secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  base::UmaHistogramEnumeration("Eche.Connection.Result.FailureReason", reason);
}

void EcheConnectionMetricsRecorder::RecordConnectionLatency(
    const base::TimeDelta latency) {
  base::UmaHistogramTimes("Eche.Connectivity.Latency", latency);
}

void EcheConnectionMetricsRecorder::RecordConnectionDuration(
    const base::TimeDelta duration) {
  base::UmaHistogramLongTimes100("Eche.Connection.Duration", duration);
}

}  // namespace ash::eche_app
