// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_frame_rate_schedule.h"

#include <vector>

#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Gt;
using ::testing::IsEmpty;

TEST(AmbientAnimationFrameRateSectionTest, Contains) {
  AmbientAnimationFrameRateSection section(.1f, .2f, kDefaultFrameInterval);
  EXPECT_FALSE(section.Contains(0.f));
  EXPECT_TRUE(section.Contains(.1f));
  EXPECT_TRUE(section.Contains(.19f));
  EXPECT_FALSE(section.Contains(.2f));
  EXPECT_FALSE(section.Contains(.3f));
}

TEST(AmbientAnimationFrameRateSectionTest, IntersectsWith) {
  AmbientAnimationFrameRateSection section(.1f, .2f, kDefaultFrameInterval);
  EXPECT_FALSE(section.IntersectsWith(
      AmbientAnimationFrameRateSection(0.f, .1f, kDefaultFrameInterval)));
  EXPECT_TRUE(section.IntersectsWith(
      AmbientAnimationFrameRateSection(.09f, .11f, kDefaultFrameInterval)));
  EXPECT_TRUE(section.IntersectsWith(
      AmbientAnimationFrameRateSection(.11f, .19f, kDefaultFrameInterval)));
  EXPECT_TRUE(section.IntersectsWith(
      AmbientAnimationFrameRateSection(.19f, .21f, kDefaultFrameInterval)));
  EXPECT_FALSE(section.IntersectsWith(
      AmbientAnimationFrameRateSection(.2f, .21f, kDefaultFrameInterval)));
  EXPECT_TRUE(section.IntersectsWith(
      AmbientAnimationFrameRateSection(.0f, .3f, kDefaultFrameInterval)));
}

TEST(AmbientAnimationFrameRateScheduleTest, BasicSchedule) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_30fps", 0.8f, 0.9f},
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
  };
  AmbientAnimationFrameRateSchedule schedule =
      BuildAmbientAnimationFrameRateSchedule(markers);
  EXPECT_THAT(schedule,
              ElementsAre(FieldsAre(0.f, 0.2f, kDefaultFrameInterval),
                          FieldsAre(0.2f, 0.4f, base::Hertz(20)),
                          FieldsAre(0.4f, 0.8f, kDefaultFrameInterval),
                          FieldsAre(0.8f, 0.9f, base::Hertz(30)),
                          FieldsAre(0.9f, Gt(1.f), kDefaultFrameInterval)));
}

TEST(AmbientAnimationFrameRateScheduleTest, NoThrottling) {
  AmbientAnimationFrameRateSchedule schedule =
      BuildAmbientAnimationFrameRateSchedule({});
  EXPECT_THAT(schedule,
              ElementsAre(FieldsAre(0.f, Gt(1.f), kDefaultFrameInterval)));
}

TEST(AmbientAnimationFrameRateScheduleTest, ScheduleCompletelyThrottled) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_30fps", 0.f, 1.f},
  };
  AmbientAnimationFrameRateSchedule schedule =
      BuildAmbientAnimationFrameRateSchedule(markers);
  EXPECT_THAT(schedule, ElementsAre(FieldsAre(0.f, Gt(1.f), base::Hertz(30))));
}

TEST(AmbientAnimationFrameRateScheduleTest, AdjacentThrottles) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
      {"_CrOS_Marker_Throttled_30fps", 0.4f, 0.6f},
  };
  AmbientAnimationFrameRateSchedule schedule =
      BuildAmbientAnimationFrameRateSchedule(markers);
  EXPECT_THAT(schedule,
              ElementsAre(FieldsAre(0.f, 0.2f, kDefaultFrameInterval),
                          FieldsAre(0.2f, 0.4f, base::Hertz(20)),
                          FieldsAre(0.4f, 0.6f, base::Hertz(30)),
                          FieldsAre(0.6f, Gt(1.f), kDefaultFrameInterval)));
}

TEST(AmbientAnimationFrameRateScheduleTest, FailsIntersectingMarkers) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
      {"_CrOS_Marker_Throttled_30fps", 0.1f, 0.21f},
  };
  EXPECT_THAT(BuildAmbientAnimationFrameRateSchedule(markers), IsEmpty());
  markers = {
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
      {"_CrOS_Marker_Throttled_30fps", 0.39f, 0.5f},
  };
  EXPECT_THAT(BuildAmbientAnimationFrameRateSchedule(markers), IsEmpty());
}

TEST(AmbientAnimationFrameRateScheduleTest, FailsInvalidMarkerTimestamps) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_20fps", -.01f, 0.1f},
  };
  EXPECT_THAT(BuildAmbientAnimationFrameRateSchedule(markers), IsEmpty());
  markers = {
      {"_CrOS_Marker_Throttled_20fps", .9f, 1.1f},
  };
  EXPECT_THAT(BuildAmbientAnimationFrameRateSchedule(markers), IsEmpty());
}

TEST(AmbientAnimationFrameRateScheduleTest, FailsInvalidFrameRate) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_0fps", 0.1f, 0.2f},
  };
  EXPECT_THAT(BuildAmbientAnimationFrameRateSchedule(markers), IsEmpty());
  markers = {
      {"_CrOS_Marker_Throttled_100fps", 0.1f, 0.2f},
  };
  EXPECT_THAT(BuildAmbientAnimationFrameRateSchedule(markers), IsEmpty());
}

TEST(AmbientAnimationFrameRateScheduleTest, IgnoresUnrelatedMarkers) {
  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_UnrelatedMarker", 0.8f, 0.9f},
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
  };
  AmbientAnimationFrameRateSchedule schedule =
      BuildAmbientAnimationFrameRateSchedule(markers);
  EXPECT_THAT(schedule,
              ElementsAre(FieldsAre(0.f, 0.2f, kDefaultFrameInterval),
                          FieldsAre(0.2f, 0.4f, base::Hertz(20)),
                          FieldsAre(0.4f, Gt(1.f), kDefaultFrameInterval)));
}

}  // namespace
}  // namespace ash
