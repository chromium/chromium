// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_LATENCY_DATA_H_
#define CC_METRICS_LATENCY_DATA_H_

#include <variant>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Used by `CompositorFrameReporter` to report event latency information to
// `CustomMetricRecorder` (e.g. for the UI compositor).
struct CC_EXPORT LatencyData {
  LatencyData(EventMetrics::EventType event_type,
              base::TimeDelta total_latency);
  ~LatencyData();

  LatencyData(const LatencyData&) = delete;
  LatencyData& operator=(const LatencyData&) = delete;

  LatencyData(LatencyData&&);
  LatencyData& operator=(LatencyData&&);

  EventMetrics::EventType event_type;
  base::TimeDelta total_latency;

  // Type of the input device if the event is a scroll or a pinch event.
  std::variant<std::monostate,
               ScrollEventMetrics::ScrollType,
               PinchEventMetrics::PinchType>
      input_type;
};

}  // namespace cc

#endif  // CC_METRICS_LATENCY_DATA_H_
