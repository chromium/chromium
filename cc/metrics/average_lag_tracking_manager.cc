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

  for (const ui::LatencyInfo& latency_info : latency_infos) {
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

    event_infos.emplace_back(
        latency_info.scroll_update_delta(),
        latency_info.predicted_scroll_update_delta(), event_timestamp,
        found_scroll_begin ? AverageLagTracker::EventType::ScrollBegin
                           : AverageLagTracker::EventType::ScrollUpdate);
  }

  if (event_infos.size() > 0)
    frame_token_to_info_.emplace_back(frame_token, std::move(event_infos));
}

void AverageLagTrackingManager::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& frame_details) {
  if (frame_details.presentation_feedback.failed()) {
    // When presentation fails, remove the current frame from (potentially, the
    // middle of) the queue; but, leave earlier frames in the queue as they
    // still might end up being presented successfully.
    for (auto submitted_frame = frame_token_to_info_.begin();
         submitted_frame != frame_token_to_info_.end(); submitted_frame++) {
      if (viz::FrameTokenGT(submitted_frame->first, frame_token))
        break;
      if (submitted_frame->first == frame_token) {
        frame_token_to_info_.erase(submitted_frame);
        break;
      }
    }
    return;
  }

  // When presentation succeeds, consider earlier frames as failed and remove
  // them from the front of the queue. Then take the list of events for the
  // current frame and remove it from the front of the queue, too.
  std::vector<AverageLagTracker::EventInfo> infos;
  while (!frame_token_to_info_.empty()) {
    auto& submitted_frame = frame_token_to_info_.front();
    if (viz::FrameTokenGT(submitted_frame.first, frame_token))
      break;
    if (submitted_frame.first == frame_token)
      infos = std::move(submitted_frame.second);
    frame_token_to_info_.pop_front();
  }

  // If there is no event, there is nothing to report.
  if (infos.empty())
    return;

  DCHECK(!frame_details.swap_timings.is_null());

  // AverageLagTracker expects events' info to be in ascending order.
  std::sort(infos.begin(), infos.end(),
            [](const AverageLagTracker::EventInfo& a,
               const AverageLagTracker::EventInfo& b) {
              return a.event_timestamp < b.event_timestamp;
            });

  for (AverageLagTracker::EventInfo& info : infos) {
    info.finish_timestamp = frame_details.presentation_feedback.timestamp;
    lag_tracker_.AddScrollEventInFrame(info);
  }
}

void AverageLagTrackingManager::Clear() {
  frame_token_to_info_.clear();
}

}  // namespace cc
