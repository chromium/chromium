// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/phone_hub_metrics_recorder.h"
#include "ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "base/metrics/histogram_functions.h"

namespace ash::phonehub {

PhoneHubMetricsRecorder::PhoneHubMetricsRecorder() = default;
PhoneHubMetricsRecorder::~PhoneHubMetricsRecorder() = default;

void PhoneHubMetricsRecorder::RecordConnectionResult(bool success) {
  base::UmaHistogramBoolean("PhoneHub.Connection.Result", success);
}

void PhoneHubMetricsRecorder::RecordConnectionFailureReason(
    secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  base::UmaHistogramEnumeration("PhoneHub.Connection.Result.FailureReason",
                                reason);
}

void PhoneHubMetricsRecorder::RecordConnectionLatency(
    const base::TimeDelta latency) {
  base::UmaHistogramTimes("PhoneHub.Connectivity.Latency", latency);
}

void PhoneHubMetricsRecorder::RecordConnectionDuration(
    const base::TimeDelta duration) {
  base::UmaHistogramLongTimes100("PhoneHub.Connection.Duration", duration);
}

}  // namespace ash::phonehub
