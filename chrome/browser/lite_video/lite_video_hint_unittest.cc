// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_hint.h"

#include "base/time/time.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "components/optimization_guide/proto/lite_video_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

void SetDurationFromTimeDelta(optimization_guide::proto::Duration* duration,
                              const base::TimeDelta& delta) {
  if (!duration)
    return;
  duration->set_seconds(delta.InSeconds());
  duration->set_nanos(delta.InNanoseconds() %
                      base::TimeDelta::FromSeconds(1).InNanoseconds());
}

TEST(LiteVideoHintTest, OptGuideMetadataHint) {
  optimization_guide::proto::LiteVideoHint hint_proto;
  base::TimeDelta throttle_delay =
      base::TimeDelta::FromSeconds(5) + base::TimeDelta::FromNanoseconds(5);
  hint_proto.set_target_downlink_bandwidth_kbps(100);
  hint_proto.set_kilobytes_to_buffer_before_throttle(10);
  SetDurationFromTimeDelta(hint_proto.mutable_target_downlink_rtt_latency(),
                           base::TimeDelta::FromSeconds(5));
  SetDurationFromTimeDelta(hint_proto.mutable_max_throttling_delay(),
                           throttle_delay);
  lite_video::LiteVideoHint hint(hint_proto);
  EXPECT_EQ(hint.target_downlink_bandwidth_kbps(), 100);
  EXPECT_EQ(hint.kilobytes_to_buffer_before_throttle(), 10);
  EXPECT_EQ(hint.target_downlink_rtt_latency(),
            base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(hint.max_throttling_delay(), throttle_delay);
}

TEST(LiteVideoHintTest, OptGuideMetadataHint_MissingFields) {
  optimization_guide::proto::LiteVideoHint hint_proto;

  lite_video::LiteVideoHint hint(hint_proto);
  EXPECT_EQ(hint.target_downlink_bandwidth_kbps(),
            lite_video::switches::GetDefaultDownlinkBandwidthKbps());
  EXPECT_EQ(hint.kilobytes_to_buffer_before_throttle(),
            lite_video::features::LiteVideoKilobytesToBufferBeforeThrottle());
  EXPECT_EQ(hint.target_downlink_rtt_latency(),
            lite_video::features::LiteVideoTargetDownlinkRTTLatency());
  EXPECT_EQ(hint.max_throttling_delay(),
            lite_video::features::LiteVideoMaxThrottlingDelay());
}
