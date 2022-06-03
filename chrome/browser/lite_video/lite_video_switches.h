// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_SWITCHES_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_SWITCHES_H_

namespace lite_video {
namespace switches {

extern const char kLiteVideoIgnoreNetworkConditions[];
extern const char kLiteVideoForceOverrideDecision[];
extern const char kLiteVideoForceCoinflipHoldback[];
extern const char kLiteVideoDefaultDownlinkBandwidthKbps[];

// Returns true if checking the network condition should be ignored.
bool ShouldIgnoreLiteVideoNetworkConditions();

// Returns true if the decision logic for whether to allow LiteVideos should be
// overridden and allow LiteVideos to be enabled for every navigation.
bool ShouldOverrideLiteVideoDecision();

// Returns true if the coinflip experiment should be set to true, resulting
// in LiteVideos being heldback.
bool ShouldForceCoinflipHoldback();

// Returns the default downlink bandwidth kbps to use when throttling media
// requests. Only used if the decision logic is skipped for testing and a
// default hint is used.
int GetDefaultDownlinkBandwidthKbps();

}  // namespace switches
}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_SWITCHES_H_
