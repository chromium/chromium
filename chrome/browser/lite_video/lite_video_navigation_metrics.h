// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_NAVIGATION_METRICS_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_NAVIGATION_METRICS_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"

namespace lite_video {

// The decision if a navigation should attempt to throttle media requests.
// This should be kept in sync with LiteVideoDecision in enums.xml.
enum class LiteVideoDecision {
  kUnknown,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist.
  kAllowed,
  // The navigation is not allowed by all types of this LiteVideoUserBlocklist.
  kNotAllowed,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist but
  // the optimization was heldback for counterfactual experiments.
  kHoldback,

  // Insert new values before this line.
  kMaxValue = kHoldback,
};

// The result of throttling on a navigation.
// This should be kept in sync with LiteVideoThrottleResult in enums.xml.
enum class LiteVideoThrottleResult {
  kUnknown,
  // LiteVideos were enabled to throttle media requests on the navigation
  // and they were not stopped due to rebuffering events.
  kThrottledWithoutStop,
  // LiteVideos were enabled to throttle media requests on the navigation
  // but they were stopped due to rebuffering events.
  kThrottleStoppedOnRebuffer,

  // Insert new values before this line.
  kMaxValue = kThrottleStoppedOnRebuffer,
};

class LiteVideoNavigationMetrics {
 public:
  LiteVideoNavigationMetrics(int64_t nav_id,
                             LiteVideoDecision decision,
                             LiteVideoBlocklistReason blocklist_reason,
                             LiteVideoThrottleResult throttle_result);
  LiteVideoNavigationMetrics(const LiteVideoNavigationMetrics& other);
  ~LiteVideoNavigationMetrics();

  int64_t nav_id() const { return nav_id_; }
  LiteVideoDecision decision() const { return decision_; }
  LiteVideoBlocklistReason blocklist_reason() const {
    return blocklist_reason_;
  }
  LiteVideoThrottleResult throttle_result() const { return throttle_result_; }

  // Returns true if the frame with |frame_id| has rebuffered too many times.
  // Updates the map holding the count of rebuffers associated with the frame
  // for the current navigation and the throttling result if the frame
  // has rebuffered too often.
  bool ShouldStopOnRebufferForFrame(int64_t frame_id);

  // Update the decision to made on applying LiteVideos to the current
  // navigation.
  void SetDecision(LiteVideoDecision decision);

  // Update the blocklist reason for the current navigation.
  void SetBlocklistReason(LiteVideoBlocklistReason blocklist_reason);

 private:
  int64_t nav_id_;
  // Map of frame ids to the count of media rebuffers events observed.
  base::flat_map<int64_t, int> frame_rebuffer_count_map_;
  LiteVideoDecision decision_;
  LiteVideoBlocklistReason blocklist_reason_;
  LiteVideoThrottleResult throttle_result_;
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_NAVIGATION_METRICS_H_
