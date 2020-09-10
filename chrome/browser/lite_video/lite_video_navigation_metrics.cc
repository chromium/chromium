// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"

namespace lite_video {

LiteVideoNavigationMetrics::LiteVideoNavigationMetrics(
    int64_t nav_id,
    LiteVideoDecision decision,
    LiteVideoBlocklistReason blocklist_reason,
    LiteVideoThrottleResult throttle_result)
    : nav_id_(nav_id),
      decision_(decision),
      blocklist_reason_(blocklist_reason),
      throttle_result_(throttle_result) {}

LiteVideoNavigationMetrics::~LiteVideoNavigationMetrics() = default;

void LiteVideoNavigationMetrics::SetThrottleResult(
    LiteVideoThrottleResult throttle_result) {
  throttle_result_ = throttle_result;
}

void LiteVideoNavigationMetrics::SetDecision(LiteVideoDecision decision) {
  decision_ = decision;
}
void LiteVideoNavigationMetrics::SetBlocklistReason(
    LiteVideoBlocklistReason blocklist_reason) {
  blocklist_reason_ = blocklist_reason;
}

}  // namespace lite_video
