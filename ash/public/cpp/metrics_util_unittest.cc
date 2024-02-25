// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/metrics_util.h"

#include <optional>

#include "base/test/bind.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace metrics_util {

TEST(MetricsUtilTest, ReportSmoothness) {
  cc::FrameSequenceMetrics::CustomReportData report_data;
  report_data.frames_dropped_v3 = 30;
  report_data.frames_expected_v3 = 60;
  constexpr int kExpectedSmoothes = 50;

  std::optional<int> reported_smoothness;
  SmoothnessCallback smoothness_callback = base::BindLambdaForTesting(
      [&](int smoothess) { reported_smoothness = smoothess; });

  struct {
    bool enable_data_collection;
    bool exclude_from_collection;
    bool expected_has_collected_data;
  } kTestCases[] = {
      {/*enable_data_collection=*/false, /*exclude_from_collection=*/false,
       /*expected_has_collected_data=*/false},
      {/*enable_data_collection=*/true, /*exclude_from_collection=*/false,
       /*expected_has_collected_data=*/true},
      {/*enable_data_collection=*/true, /*exclude_from_collection=*/true,
       /*expected_has_collected_data=*/false},
  };

  for (auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "enable_data_collection=" << test.enable_data_collection
                 << ", exclude_from_collection=" << test.exclude_from_collection
                 << ", expected_has_collected_data="
                 << test.expected_has_collected_data);

    if (test.enable_data_collection)
      StartDataCollection();

    reported_smoothness.reset();
    ReportCallback report_callback =
        ForSmoothnessV3(smoothness_callback, test.exclude_from_collection);
    report_callback.Run(report_data);
    ASSERT_TRUE(reported_smoothness.has_value());
    EXPECT_EQ(kExpectedSmoothes, reported_smoothness.value());

    if (test.enable_data_collection) {
      auto collected_data = StopDataCollection();
      EXPECT_EQ(test.expected_has_collected_data, !collected_data.empty());
    }
  }
}

}  // namespace metrics_util
}  // namespace ash
