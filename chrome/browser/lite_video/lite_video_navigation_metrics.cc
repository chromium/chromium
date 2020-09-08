// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/lite_video/lite_video_features.h"

namespace lite_video {

LiteVideoNavigationMetrics::LiteVideoNavigationMetrics(
    int64_t nav_id,
    LiteVideoDecision decision,
    LiteVideoBlocklistReason blocklist_reason,
    LiteVideoThrottleResult throttle_result)
    : nav_id_(nav_id),
      decision_(decision),
      blocklist_reason_(blocklist_reason),
      throttle_result_(throttle_result) {
  frame_rebuffer_count_map_ = {};
}

LiteVideoNavigationMetrics::LiteVideoNavigationMetrics(
    const LiteVideoNavigationMetrics& other) = default;

LiteVideoNavigationMetrics::~LiteVideoNavigationMetrics() {
  if (frame_rebuffer_count_map_.size() > 0) {
    UMA_HISTOGRAM_COUNTS_1000(
        "LiteVideo.NavigationMetrics.FrameRebufferMapSize",
        frame_rebuffer_count_map_.size());
  }
}

bool LiteVideoNavigationMetrics::ShouldStopOnRebufferForFrame(
    int64_t frame_id) {
  auto it = frame_rebuffer_count_map_.find(frame_id);
  if (it == frame_rebuffer_count_map_.end()) {
    frame_rebuffer_count_map_[frame_id] = 1;
  } else {
    frame_rebuffer_count_map_[frame_id]++;
  }

  if (frame_rebuffer_count_map_[frame_id] >=
      features::GetMaxRebuffersPerFrame()) {
    throttle_result_ = LiteVideoThrottleResult::kThrottleStoppedOnRebuffer;
    return true;
  }
  return false;
}

void LiteVideoNavigationMetrics::SetDecision(LiteVideoDecision decision) {
  decision_ = decision;
}
void LiteVideoNavigationMetrics::SetBlocklistReason(
    LiteVideoBlocklistReason blocklist_reason) {
  blocklist_reason_ = blocklist_reason;
}

}  // namespace lite_video
