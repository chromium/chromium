// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_switches.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace lite_video {
namespace switches {

// Overrides the network conditions checks for LiteVideos.
const char kLiteVideoIgnoreNetworkConditions[] =
    "lite-video-ignore-network-conditions";

// Overrides all the LiteVideo decision logic to allow it on every navigation.
// This causes LiteVideos to ignore the hints, user blocklist, and
// network condition.
const char kLiteVideoForceOverrideDecision[] =
    "lite-video-force-override-decision";

// Forces the coinflip used for a counterfactual experiment to be true.
const char kLiteVideoForceCoinflipHoldback[] =
    "lite-video-force-coinflip-holdback";

// The default downlink bandwidth estimate used for throttling media requests.
// Only used when forcing LiteVideos to be allowed.
const char kLiteVideoDefaultDownlinkBandwidthKbps[] =
    "lite-video-default-downlink-bandwidth-kbps";

bool ShouldIgnoreLiteVideoNetworkConditions() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kLiteVideoIgnoreNetworkConditions);
}

bool ShouldOverrideLiteVideoDecision() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kLiteVideoForceOverrideDecision);
}

bool ShouldForceCoinflipHoldback() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kLiteVideoForceCoinflipHoldback);
}

int GetDefaultDownlinkBandwidthKbps() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kLiteVideoDefaultDownlinkBandwidthKbps)) {
    int downlink_bandwidth_kbps;
    if (base::StringToInt(command_line->GetSwitchValueASCII(
                              switches::kLiteVideoDefaultDownlinkBandwidthKbps),
                          &downlink_bandwidth_kbps)) {
      return downlink_bandwidth_kbps;
    }
  }
  return 400;
}

}  // namespace switches
}  // namespace lite_video
