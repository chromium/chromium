// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/average_lag_tracking_manager.h"

#include <algorithm>

#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/latency/latency_info.h"

namespace cc {

AverageLagTrackingManager::AverageLagTrackingManager() = default;

AverageLagTrackingManager::~AverageLagTrackingManager() {
  // The map must contain only frames that haven't been presented (i.e. did not
  // get a presentation feedback yet). Thus, at a given point in time, more than
  // a handful (actually around 2) of frames without feedback is unexpected.
  DCHECK_LE(frame_token_to_info_.size(), 20u);
}

void AverageLagTrackingManager::CollectScrollEventsFromFrame(
    uint32_t frame_token,
    const std::vector<ui::LatencyInfo>& latency_infos) {
  std::vector<AverageLagTracker::EventInfo> event_infos;

  for (ui::LatencyInfo latency_info : latency_infos) {
    if (latency_info.source_event_type() != ui::SourceEventType::TOUCH)
      continue;

    bool found_scroll_begin = latency_info.FindLatency(
        ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        nullptr);
    bool found_scroll_update = latency_info.FindLatency(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr);

    if (!found_scroll_begin && !found_scroll_update)
      continue;

    base::TimeTicks event_timestamp;
    bool found_event = latency_info.FindLatency(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT,
        &event_timestamp);
    DCHECK(found_event);

    AverageLagTracker::EventInfo event_info(
        latency_info.trace_id(), latency_info.scroll_update_delta(),
        latency_info.predicted_scroll_update_delta(), event_timestamp,
        found_scroll_begin == true
            ? AverageLagTracker::EventType::ScrollBegin
            : AverageLagTracker::EventType::ScrollUpdate);

    event_infos.push_back(event_info);
  }

  if (event_infos.size() > 0)
    frame_token_to_info_.push_back(std::make_pair(frame_token, event_infos));
}

void AverageLagTrackingManager::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& frame_details) {
  // Erase all previous frames that haven't received a feedback and get the
  // current |frame_token| list of events.
  std::vector<AverageLagTracker::EventInfo> infos;
  while (!frame_token_to_info_.empty() &&
         !viz::FrameTokenGT(frame_token_to_info_.front().first, frame_token)) {
    if (frame_token_to_info_.front().first == frame_token)
      infos = frame_token_to_info_.front().second;

    frame_token_to_info_.pop_front();
  }

  if (infos.size() == 0)
    return;

  if (!frame_details.presentation_feedback.failed()) {
    DCHECK(!frame_details.swap_timings.is_null());

    // Sorts data by trace_id because |infos| can be in non-asceding order
    // (ascending order of trace_id/time is required by AverageLagTracker).
    std::sort(infos.begin(), infos.end(),
              [](const AverageLagTracker::EventInfo& a,
                 const AverageLagTracker::EventInfo& b) {
                return a.trace_id < b.trace_id;
              });

    for (AverageLagTracker::EventInfo info : infos) {
      info.finish_timestamp = frame_details.presentation_feedback.timestamp;
      lag_tracker_.AddScrollEventInFrame(info);
    }
  }
}

void AverageLagTrackingManager::Clear() {
  frame_token_to_info_.clear();
}
}  // namespace cc
