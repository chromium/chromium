// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_tab_data.h"

#include <memory>
#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace actor {
namespace {

using ::testing::ReturnRef;

constexpr std::string_view
    kActorPageContextAPCComparisonActorIsIdenticalToPreviousFetchMetricName =
        "Actor.PageContext.APC.Comparison.Actor.IsIdenticalToPreviousFetch";
constexpr std::string_view
    kActorPageContextAPCComparisonGlicIsIdenticalToPreviousFetchMetricName =
        "Actor.PageContext.APC.Comparison.Glic.IsIdenticalToPreviousFetch";

class ActorTabDataTest : public testing::Test {
 public:
  ActorTabDataTest() = default;
  ~ActorTabDataTest() override = default;

  void SetUp() override {
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
  }

 protected:
  tabs::MockTabInterface mock_tab_;
  ::ui::UnownedUserDataHost user_data_host_;
};

TEST_F(ActorTabDataTest, DidObserveContentRecordsMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicActorApcComparison, {{"sampling-rate", "1.0"}});

  base::HistogramTester histogram_tester;
  auto tab_data_ptr = std::make_unique<ActorTabData>(&mock_tab_);
  ActorTabData* tab_data = ActorTabData::From(&mock_tab_);
  ASSERT_TRUE(tab_data);

  optimization_guide::proto::AnnotatedPageContent content1;
  content1.set_tab_id(1);
  content1.mutable_main_frame_data()->set_title("title1");

  // First observation should not record any metrics.
  tab_data->DidObserveContent(content1, ApcSource::kActor);
  histogram_tester.ExpectTotalCount(
      kActorPageContextAPCComparisonActorIsIdenticalToPreviousFetchMetricName,
      0);

  // Second observation with identical content should record 'true'.
  tab_data->DidObserveContent(content1, ApcSource::kActor);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextAPCComparisonActorIsIdenticalToPreviousFetchMetricName,
      true, 1);

  // Third observation with different content should record 'false'.
  optimization_guide::proto::AnnotatedPageContent content2;
  content2.set_tab_id(1);
  content2.mutable_main_frame_data()->set_title("title2");
  tab_data->DidObserveContent(content2, ApcSource::kActor);
  histogram_tester.ExpectBucketCount(
      kActorPageContextAPCComparisonActorIsIdenticalToPreviousFetchMetricName,
      false, 1);

  // Observation from different source (Glic) should compare with the last fetch
  // (Actor). Current last content is content2.
  tab_data->DidObserveContent(content2, ApcSource::kGlic);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextAPCComparisonGlicIsIdenticalToPreviousFetchMetricName,
      true, 1);

  // Another fetch from Glic, different from previous Glic (which was same as
  // Actor's content2).
  tab_data->DidObserveContent(content1, ApcSource::kGlic);
  histogram_tester.ExpectBucketCount(
      kActorPageContextAPCComparisonGlicIsIdenticalToPreviousFetchMetricName,
      false, 1);
}

TEST_F(ActorTabDataTest, DidObserveContentRecordsNoMetricsWhenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kGlicActorApcComparison);

  base::HistogramTester histogram_tester;
  auto tab_data_ptr = std::make_unique<ActorTabData>(&mock_tab_);
  ActorTabData* tab_data = ActorTabData::From(&mock_tab_);

  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(1);
  content.mutable_main_frame_data()->set_title("title1");

  tab_data->DidObserveContent(content, ApcSource::kActor);
  tab_data->DidObserveContent(content, ApcSource::kActor);
  tab_data->DidObserveContent(content, ApcSource::kGlic);

  histogram_tester.ExpectTotalCount(
      kActorPageContextAPCComparisonActorIsIdenticalToPreviousFetchMetricName,
      0);
  histogram_tester.ExpectTotalCount(
      kActorPageContextAPCComparisonGlicIsIdenticalToPreviousFetchMetricName,
      0);
}

}  // namespace
}  // namespace actor
