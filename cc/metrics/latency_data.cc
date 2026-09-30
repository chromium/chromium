// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/latency_data.h"

namespace cc {

LatencyData::LatencyData(EventMetrics::EventType event_type,
                         base::TimeDelta total_latency)
    : event_type(event_type), total_latency(total_latency) {}

LatencyData::~LatencyData() = default;

LatencyData::LatencyData(LatencyData&&) = default;
LatencyData& LatencyData::operator=(LatencyData&&) = default;

}  // namespace cc
