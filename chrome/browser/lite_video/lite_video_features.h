// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_FEATURES_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_FEATURES_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "net/nqe/effective_connection_type.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Value;
}  // namespace base

namespace lite_video {
namespace features {

// Whether the LiteVideo feature that throttles media requests to reduce
// adaptive bitrates of media streams is enabled. Currently disabled by default.
bool IsLiteVideoEnabled();

// Whether the LiteVideo coinflip experiment is enabled. The coinflip
// experiment is a counterfactual experiment that decides whether LiteVideos
// should be heldback on a per navigation basis.
bool IsCoinflipExperimentEnabled();

// Whether LiteVideo should rely on the optimization guide for hints.
bool LiteVideoUseOptimizationGuide();

// Return the origins that are whitelisted for using the LiteVideo optimization
// and the parameters needed to throttle media requests for that origin.
base::Optional<base::Value> GetLiteVideoOriginHintsFromFieldTrial();

// The target for of the round-trip time for media requests used when
// throttling media requests.
base::TimeDelta LiteVideoTargetDownlinkRTTLatency();

// The number of kilobytes to be buffered before starting to throttle media
// requests.
int LiteVideoKilobytesToBufferBeforeThrottle();

// The maximum delay a throttle can introduce for a media request.
base::TimeDelta LiteVideoMaxThrottlingDelay();

// The maximum number of hosts maintained for each blocklist for the LiteVideo
// optimization.
size_t MaxUserBlocklistHosts();

// The duration which a host will remain blocklisted from having media requests
// throttled based on user opt-outs.
base::TimeDelta UserBlocklistHostDuration();

// The number of opt-out events for a host to be considered to be blocklisted.
int UserBlocklistOptOutHistoryThreshold();

// The current version of the LiteVideo user blocklist.
int LiteVideoBlocklistVersion();

// The minimum effective connection type that LiteVideos should be attempted
// on.
net::EffectiveConnectionType MinLiteVideoECT();

// The maximum number of hints the LiteVideoDecider should cache locally
// for reuse by subframes.
int MaxOptimizationGuideHintCacheSize();

// Return the set of hosts that LiteVideos are permanently blocked from
// being applied on.
base::flat_set<std::string> GetLiteVideoPermanentBlocklist();

// Return if the page transition is forward-back and LiteVideos
// are not allowed on those navigations.
bool IsLiteVideoNotAllowedForPageTransition(ui::PageTransition page_transition);

// The number of media rebuffers before all throttling within the frame
// should be stopped.
int GetMaxRebuffersPerFrame();

}  // namespace features
}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_FEATURES_H_
