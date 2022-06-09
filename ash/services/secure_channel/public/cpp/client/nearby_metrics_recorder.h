// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_METRICS_RECORDER_H_
#define ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace ash::secure_channel {

// Interface for recording connection metrics.
class NearbyMetricsRecorder {
 public:
  NearbyMetricsRecorder() = default;
  virtual ~NearbyMetricsRecorder() = default;

  virtual void RecordConnectionResult(bool success) = 0;
  virtual void RecordConnectionLatency(const base::TimeDelta& latency) = 0;
  virtual void RecordConnectionDuration(const base::TimeDelta& duration) = 0;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_METRICS_RECORDER_H_
