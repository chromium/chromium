// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/average_lag_tracking_manager.h"

#include <algorithm>
#include <memory>

#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace cc {
namespace {

void AddEventInfoFromEventMetricsList(
    const EventMetrics::List& events_metrics,
    std::vector<AverageLagTracker::EventInfo>* event_infos) {
  for (const std::unique_ptr<EventMetrics>& event_metrics : events_metrics) {
    EventMetrics::EventType type = event_metrics->type();
    if (type != EventMetrics::EventType::kFirstGestureScrollUpdate &&
        type != EventMetrics::EventType::kGestureScrollUpdate) {
      continue;
    }

    auto* scroll_update_metrics = event_metrics->AsScrollUpdate();
    DCHECK(scroll_update_metrics);
    if (scroll_update_metrics->scroll_type() !=
        ScrollEventMetrics::ScrollType::kTouchscreen) {
      continue;
    }

    event_infos->emplace_back(
        scroll_update_metrics->delta(),
        scroll_update_metrics->predicted_delta(),
        scroll_update_metrics->last_timestamp(),
        type == EventMetrics::EventType::kFirstGestureScrollUpdate
            ? AverageLagTracker::EventType::kScrollbegin
            : AverageLagTracker::EventType::kScrollupdate);
  }
}

}  // namespace

AverageLagTrackingManager::AverageLagTrackingManager() = default;

AverageLagTrackingManager::~AverageLagTrackingManager() {
  // The map must contain only frames that haven't been presented (i.e. did not
  // get a presentation feedback yet). Thus, at a given point in time, more than
  // a handful (actually around 2) of frames without feedback is unexpected.
  DCHECK_LE(frame_token_to_info_.size(), 20u);
}

void AverageLagTrackingManager::CollectScrollEventsFromFrame(
    uint32_t frame_token,
    const EventMetricsSet& events_metrics) {
  std::vector<AverageLagTracker::EventInfo> event_infos;

  // A scroll event can be handled either on the main or the compositor thread
  // (not both). So, both lists of metrics from the main and the compositor
  // thread might contain interesting scroll events and we should collect
  // information about scroll events from both. We are not worried about
  // ordering of the events at this point. If the frame is presented, events
  // for the frame will be sorted and fed into `AverageLagTracker` in order.
  AddEventInfoFromEventMetricsList(events_metrics.main_event_metrics,
                                   &event_infos);
  AddEventInfoFromEventMetricsList(events_metrics.impl_event_metrics,
                                   &event_infos);

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
