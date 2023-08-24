// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using cast_channel::ReceiverAppType;
using testing::ElementsAre;

namespace media_router {

TEST(CastMediaRouteProviderMetricsTest, RecordAppAvailabilityResult) {
  base::HistogramTester tester;

  RecordAppAvailabilityResult(
      cast_channel::GetAppAvailabilityResult::kAvailable,
      base::Milliseconds(111));
  RecordAppAvailabilityResult(
      cast_channel::GetAppAvailabilityResult::kUnavailable,
      base::Milliseconds(222));
  tester.ExpectTimeBucketCount(kHistogramAppAvailabilitySuccess,
                               base::Milliseconds(111), 1);
  tester.ExpectTimeBucketCount(kHistogramAppAvailabilitySuccess,
                               base::Milliseconds(222), 1);

  RecordAppAvailabilityResult(cast_channel::GetAppAvailabilityResult::kUnknown,
                              base::Milliseconds(333));
  tester.ExpectTimeBucketCount(kHistogramAppAvailabilityFailure,
                               base::Milliseconds(333), 1);
}

TEST(CastMediaRouteProviderMetricsTest, RecordLaunchSessionResponseAppType) {
  base::HistogramTester tester;

  base::Value web_val("WEB");
  base::Value atv_val("ANDROID_TV");
  base::Value other_val("OTHER");
  base::Value invalid_val("Invalid");

  RecordLaunchSessionResponseAppType(&web_val);
  RecordLaunchSessionResponseAppType(&web_val);
  RecordLaunchSessionResponseAppType(&atv_val);
  RecordLaunchSessionResponseAppType(&other_val);
  RecordLaunchSessionResponseAppType(&invalid_val);

  tester.ExpectBucketCount(kHistogramCastAppType, ReceiverAppType::kAndroidTv,
                           1);
  tester.ExpectBucketCount(kHistogramCastAppType, ReceiverAppType::kWeb, 2);
  tester.ExpectBucketCount(kHistogramCastAppType, ReceiverAppType::kOther, 2);
}

}  // namespace media_router
