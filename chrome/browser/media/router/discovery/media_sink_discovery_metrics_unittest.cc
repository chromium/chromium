// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace media_router {

TEST(DialDeviceCountMetricsTest, RecordDeviceCountsIfNeeded) {
  DialDeviceCountMetrics metrics;
  base::SimpleTestClock clock;
  metrics.SetClockForTest(&clock);
  base::HistogramTester tester;
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 0);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 0);

  // Only record one count within one hour.
  clock.SetNow(base::Time::Now());
  metrics.RecordDeviceCountsIfNeeded(6, 10);
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 1);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 10, 1);

  // Record another count.
  clock.Advance(base::Hours(2));
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 2);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 2);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 7, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 10, 2);
}

TEST(CastDeviceCountMetricsTest, RecordDeviceCountsIfNeeded) {
  CastDeviceCountMetrics metrics;
  base::SimpleTestClock clock;
  metrics.SetClockForTest(&clock);
  base::HistogramTester tester;
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 0);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 0);

  // Only record one count within one hour.
  clock.SetNow(base::Time::Now());
  metrics.RecordDeviceCountsIfNeeded(6, 10);
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 1);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 10, 1);

  // Record another count.
  clock.Advance(base::Hours(2));
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 2);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 2);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 7, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 10, 2);
}

TEST(CastAnalyticsTest, RecordCastChannelConnectResult) {
  const MediaRouterChannelConnectResults success =
      MediaRouterChannelConnectResults::SUCCESS;
  const MediaRouterChannelConnectResults failure =
      MediaRouterChannelConnectResults::FAILURE;

  base::HistogramTester tester;
  tester.ExpectTotalCount(CastAnalytics::kHistogramCastChannelConnectResult, 0);
  CastAnalytics::RecordCastChannelConnectResult(success);
  CastAnalytics::RecordCastChannelConnectResult(failure);
  CastAnalytics::RecordCastChannelConnectResult(success);
  tester.ExpectTotalCount(CastAnalytics::kHistogramCastChannelConnectResult, 3);
  EXPECT_THAT(
      tester.GetAllSamples(CastAnalytics::kHistogramCastChannelConnectResult),
      ElementsAre(Bucket(static_cast<int>(failure), 1),
                  Bucket(static_cast<int>(success), 2)));
}

TEST(CastAnalyticsTest, RecordDeviceChannelError) {
  base::HistogramTester tester;
  const MediaRouterChannelError error1 =
      MediaRouterChannelError::AUTHENTICATION;
  const MediaRouterChannelError error2 = MediaRouterChannelError::CONNECT;
  const MediaRouterChannelError error3 =
      MediaRouterChannelError::GENERAL_CERTIFICATE;

  tester.ExpectTotalCount(CastAnalytics::kHistogramCastChannelError, 0);
  CastAnalytics::RecordDeviceChannelError(error1);
  CastAnalytics::RecordDeviceChannelError(error2);
  CastAnalytics::RecordDeviceChannelError(error2);
  CastAnalytics::RecordDeviceChannelError(error3);
  tester.ExpectTotalCount(CastAnalytics::kHistogramCastChannelError, 4);
  EXPECT_THAT(tester.GetAllSamples(CastAnalytics::kHistogramCastChannelError),
              ElementsAre(Bucket(static_cast<int>(error1), 1),
                          Bucket(static_cast<int>(error2), 2),
                          Bucket(static_cast<int>(error3), 1)));
}

TEST(CastAnalyticsTest, RecordDeviceChannelOpenDuration) {
  base::HistogramTester tester;
  const base::TimeDelta delta = base::Milliseconds(10);

  tester.ExpectTotalCount(CastAnalytics::kHistogramCastMdnsChannelOpenSuccess,
                          0);
  CastAnalytics::RecordDeviceChannelOpenDuration(true, delta);
  tester.ExpectUniqueSample(CastAnalytics::kHistogramCastMdnsChannelOpenSuccess,
                            delta.InMilliseconds(), 1);

  tester.ExpectTotalCount(CastAnalytics::kHistogramCastMdnsChannelOpenFailure,
                          0);
  CastAnalytics::RecordDeviceChannelOpenDuration(false, delta);
  tester.ExpectUniqueSample(CastAnalytics::kHistogramCastMdnsChannelOpenFailure,
                            delta.InMilliseconds(), 1);
}

TEST(WiredDisplayDeviceCountMetricsTest, RecordWiredDisplaySinkCount) {
  base::HistogramTester tester;
  WiredDisplayDeviceCountMetrics metrics;
  tester.ExpectTotalCount(
      WiredDisplayDeviceCountMetrics::kHistogramWiredDisplayDeviceCount, 0);

  // Only the first argument, the available sink count, is recorded.
  metrics.RecordDeviceCounts(1, 1);
  metrics.RecordDeviceCounts(200, 200);
  metrics.RecordDeviceCounts(0, 0);
  metrics.RecordDeviceCounts(25, 30);
  metrics.RecordDeviceCounts(1, 0);

  tester.ExpectTotalCount(
      WiredDisplayDeviceCountMetrics::kHistogramWiredDisplayDeviceCount, 5);
  EXPECT_THAT(
      tester.GetAllSamples(
          WiredDisplayDeviceCountMetrics::kHistogramWiredDisplayDeviceCount),
      ElementsAre(
          Bucket(0, 1), Bucket(1, 2), Bucket(25, 1),
          Bucket(100, 1)));  // Counts over 100 are all put in the 100 bucket.
}

}  // namespace media_router
