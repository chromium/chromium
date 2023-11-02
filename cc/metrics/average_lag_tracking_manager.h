// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_AVERAGE_LAG_TRACKING_MANAGER_H_
#define CC_METRICS_AVERAGE_LAG_TRACKING_MANAGER_H_

#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "cc/cc_export.h"
#include "cc/metrics/average_lag_tracker.h"
#include "cc/metrics/event_metrics.h"

namespace viz {
struct FrameTimingDetails;
}  // namespace viz

namespace cc {

// A helper to decouple the `EventMetrics` and the `AverageLagTracker`.
class CC_EXPORT AverageLagTrackingManager {
 public:
  AverageLagTrackingManager();
  ~AverageLagTrackingManager();

  // Disallow copy and assign.
  AverageLagTrackingManager(const AverageLagTrackingManager&) = delete;
  AverageLagTrackingManager& operator=(AverageLagTrackingManager const&) =
      delete;

  // Adds all the eligible events in the collection |infos| to the |frame_token|
  // wait list.
  void CollectScrollEventsFromFrame(uint32_t frame_token,
                                    const EventMetricsSet& events_metrics);

  // Sends all pending events in the |frame_token| list to the
  // AverageLagTracker, given its |frame_details|.
  void DidPresentCompositorFrame(uint32_t frame_token,
                                 const viz::FrameTimingDetails& frame_details);

  // Clears the list of events |frame_token_to_info_| if the current frames wont
  // receive a presentation feedback (e.g. LayerTreeFrameSink loses context)
  void Clear();

 private:
  // Tracker for the AverageLagPresentation metrics that uses the presentation
  // feedback time as an approximation for the time the users sees the frame on
  // the screen.
  AverageLagTracker lag_tracker_;

  // List of events (vector) per frame (uint32_t |frame_token|) to submit to the
  // lag trackers when DidPresentCompositorFrame is called for a |frame_token|.
  base::circular_deque<
      std::pair<uint32_t, std::vector<AverageLagTracker::EventInfo>>>
      frame_token_to_info_;
};

}  // namespace cc

#endif  // CC_METRICS_AVERAGE_LAG_TRACKING_MANAGER_H_
