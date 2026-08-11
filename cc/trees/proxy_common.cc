// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_common.h"

#include "base/trace_event/trace_id_helper.h"
#include "cc/base/features.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/mutator_host.h"

namespace cc {

BeginMainFrameAndCommitState::BeginMainFrameAndCommitState()
    : trace_id(base::trace_event::GetNextGlobalTraceId()) {}

BeginMainFrameAndCommitState::~BeginMainFrameAndCommitState() = default;

// Returns the factor by which we are currently throttled. E.g. a return value
// of 2 means we are throttled down to 1/2 of the normal framerate. A return
// value of 0 means we are unthrottled.
int GetThrottlingFactor(int consecutive_no_damage_main_frames) {
  // Always unthrottled if the feature is disabled.
  if (!base::FeatureList::IsEnabled(
          features::kThrottleRepeatedNoDamageFrames)) {
    return 0;
  }

  // See comments related to |kThrottleRepeatedNoDamageFrames| for meanings of
  // the parameters.
  const int threshold1 =
      features::kThrottleRepeatedNoDamageFramesThreshold1.Get();
  const int threshold2 =
      features::kThrottleRepeatedNoDamageFramesThreshold2.Get();
  const int factor1 =
      features::kThrottleRepeatedNoDamageFramesIntervalFactor1.Get();
  const int factor2 =
      features::kThrottleRepeatedNoDamageFramesIntervalFactor2.Get();

  // We have two levels of throttling:
  //
  // The first triggers after |threshold1| no-damage frames in a row, after
  // which we are throttled by a factor of |factor1|.
  //
  // The second triggers after a further |threshold2| no-damage frames in a
  // row, after which we are further throttled by a factor of |factor2|.
  //
  // Below |threshold1|, we are not throttled.
  const int count = consecutive_no_damage_main_frames;
  if (count >= threshold1 + threshold2) {
    return factor1 * factor2;
  }

  if (count >= threshold1) {
    return factor1;
  }

  return 0;
}

}  // namespace cc
