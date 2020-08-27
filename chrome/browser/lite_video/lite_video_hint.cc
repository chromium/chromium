// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_hint.h"

#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_switches.h"

namespace {

base::TimeDelta GetTimeDeltaFromDuration(
    const optimization_guide::proto::Duration& duration) {
  base::TimeDelta delta;
  if (duration.has_seconds())
    delta += base::TimeDelta::FromSeconds(duration.seconds());
  if (duration.has_nanos())
    delta += base::TimeDelta::FromNanoseconds(duration.nanos());
  return delta;
}

}  // namespace

namespace lite_video {

LiteVideoHint::LiteVideoHint(int target_downlink_bandwidth_kbps,
                             base::TimeDelta target_downlink_rtt_latency,
                             int kilobytes_to_buffer_before_throttle,
                             base::TimeDelta max_throttling_delay)
    : target_downlink_bandwidth_kbps_(target_downlink_bandwidth_kbps),
      target_downlink_rtt_latency_(target_downlink_rtt_latency),
      kilobytes_to_buffer_before_throttle_(kilobytes_to_buffer_before_throttle),
      max_throttling_delay_(max_throttling_delay) {}

LiteVideoHint::LiteVideoHint(
    const optimization_guide::proto::LiteVideoHint& hint_proto) {
  target_downlink_bandwidth_kbps_ =
      hint_proto.has_target_downlink_bandwidth_kbps()
          ? hint_proto.target_downlink_bandwidth_kbps()
          : switches::GetDefaultDownlinkBandwidthKbps();
  target_downlink_rtt_latency_ =
      hint_proto.has_target_downlink_rtt_latency()
          ? GetTimeDeltaFromDuration(hint_proto.target_downlink_rtt_latency())
          : features::LiteVideoTargetDownlinkRTTLatency();
  kilobytes_to_buffer_before_throttle_ =
      hint_proto.has_kilobytes_to_buffer_before_throttle()
          ? hint_proto.kilobytes_to_buffer_before_throttle()
          : features::LiteVideoKilobytesToBufferBeforeThrottle();
  max_throttling_delay_ =
      hint_proto.has_max_throttling_delay()
          ? GetTimeDeltaFromDuration(hint_proto.max_throttling_delay())
          : features::LiteVideoMaxThrottlingDelay();
}
}  // namespace lite_video
