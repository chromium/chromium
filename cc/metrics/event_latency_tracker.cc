// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_latency_tracker.h"

namespace cc {

EventLatencyTracker::LatencyData::LatencyData(
    EventMetrics::EventType event_type,
    base::TimeDelta total_latency)
    : event_type(event_type), total_latency(total_latency) {}

EventLatencyTracker::LatencyData::~LatencyData() = default;

EventLatencyTracker::LatencyData::LatencyData(LatencyData&&) = default;
EventLatencyTracker::LatencyData& EventLatencyTracker::LatencyData::operator=(
    LatencyData&&) = default;

EventLatencyTracker::EventLatencyTracker() = default;
EventLatencyTracker::~EventLatencyTracker() = default;

}  // namespace cc
