// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_LATENCY_TRACKER_H_
#define CC_METRICS_EVENT_LATENCY_TRACKER_H_

#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cc {

// Used by `CompositorFrameReporter` to report event latency information back to
// `LayerTreeHostImpl` and eventually to UI compositor.
class CC_EXPORT EventLatencyTracker {
 public:
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
    absl::variant<absl::monostate,
                  ScrollEventMetrics::ScrollType,
                  PinchEventMetrics::PinchType>
        input_type;
  };

  EventLatencyTracker();
  virtual ~EventLatencyTracker();

  EventLatencyTracker(const EventLatencyTracker&) = delete;
  EventLatencyTracker& operator=(const EventLatencyTracker&) = delete;

  // Called every time a frame has latency metrics to report for events.
  virtual void ReportEventLatency(std::vector<LatencyData> latencies) = 0;
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_LATENCY_TRACKER_H_
