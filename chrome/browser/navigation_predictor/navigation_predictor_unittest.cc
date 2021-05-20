// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "url/gurl.h"

namespace {

class NavigationPredictorTest : public ChromeRenderViewHostTestHarness {
 public:
  NavigationPredictorTest() = default;
  ~NavigationPredictorTest() override = default;

  // Helper function to generate mojom metrics.
  blink::mojom::AnchorElementMetricsPtr CreateMetricsPtr() {
    auto metrics = blink::mojom::AnchorElementMetrics::New();
    metrics->anchor_id = next_id_++;
    metrics->source_url = GURL("https://example.com");
    metrics->target_url = GURL("https://google.com");
    metrics->ratio_area = 0.1;
    return metrics;
  }

  gfx::Size GetDefaultViewport() { return gfx::Size(600, 800); }

  blink::mojom::AnchorElementMetricsHost* predictor_service() const {
    return predictor_service_.get();
  }

 protected:
  void SetUp() override {
    // To avoid tsan data race test flakes, this needs to happen before
    // ChromeRenderViewHostTestHarness::SetUp causes tasks on other threads
    // to check if a feature is enabled.
    SetupFieldTrial();

    ChromeRenderViewHostTestHarness::SetUp();
    new NavigationPredictor(main_rfh(),
                            predictor_service_.BindNewPipeAndPassReceiver());
  }

  void SetupFieldTrial() {
    if (field_trial_initiated_)
      return;

    field_trial_initiated_ = true;

    // Report all anchors to avoid non-deterministic behavior.
    std::map<std::string, std::string> params;
    params["random_anchor_sampling_period"] = "1";

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service_;

  int next_id_ = 0;
  bool field_trial_initiated_ = false;
};

}  // namespace

// Basic test to check the ReportNewAnchorElements method aggregates
// metric data correctly.
TEST_F(NavigationPredictorTest, ReportNewAnchorElements) {
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->ratio_distance_top_to_visible_top = 10;
  metrics[0]->viewport_size = GetDefaultViewport();
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  PageAnchorsMetricsObserver::AnchorsData::CreateForWebContents(web_contents());
  PageAnchorsMetricsObserver::AnchorsData* data =
      PageAnchorsMetricsObserver::AnchorsData::FromWebContents(web_contents());
  EXPECT_EQ(1u, data->number_of_anchors_);
  EXPECT_EQ(0u, data->number_of_anchors_contains_image_);
  EXPECT_EQ(0u, data->number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data->number_of_anchors_same_host_);
  EXPECT_EQ(0u, data->number_of_anchors_url_incremented_);
  EXPECT_EQ(10, data->total_clickable_space_);
  EXPECT_EQ(10 * 100, data->MedianLinkLocation());
  EXPECT_EQ(GetDefaultViewport().height(), data->viewport_height_);
  EXPECT_EQ(GetDefaultViewport().width(), data->viewport_width_);

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->contains_image = true;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, data->number_of_anchors_);
  EXPECT_EQ(1u, data->number_of_anchors_contains_image_);
  EXPECT_EQ(0u, data->number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data->number_of_anchors_same_host_);
  EXPECT_EQ(0u, data->number_of_anchors_url_incremented_);
  EXPECT_EQ(20, data->total_clickable_space_);
  EXPECT_EQ(5 * 100, data->MedianLinkLocation());

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->is_in_iframe = true;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, data->number_of_anchors_);
  EXPECT_EQ(1u, data->number_of_anchors_contains_image_);
  EXPECT_EQ(1u, data->number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data->number_of_anchors_same_host_);
  EXPECT_EQ(0u, data->number_of_anchors_url_incremented_);
  EXPECT_EQ(30, data->total_clickable_space_);
  EXPECT_EQ(0, data->MedianLinkLocation());

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->is_same_host = true;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4u, data->number_of_anchors_);
  EXPECT_EQ(1u, data->number_of_anchors_contains_image_);
  EXPECT_EQ(1u, data->number_of_anchors_in_iframe_);
  EXPECT_EQ(1u, data->number_of_anchors_same_host_);
  EXPECT_EQ(0u, data->number_of_anchors_url_incremented_);
  EXPECT_EQ(40, data->total_clickable_space_);
  EXPECT_EQ(0, data->MedianLinkLocation());

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->is_url_incremented_by_one = true;
  metrics[0]->ratio_area = 0.05;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, data->number_of_anchors_);
  EXPECT_EQ(1u, data->number_of_anchors_contains_image_);
  EXPECT_EQ(1u, data->number_of_anchors_in_iframe_);
  EXPECT_EQ(1u, data->number_of_anchors_same_host_);
  EXPECT_EQ(1u, data->number_of_anchors_url_incremented_);
  EXPECT_EQ(45, data->total_clickable_space_);
  EXPECT_EQ(0, data->MedianLinkLocation());
}

TEST_F(NavigationPredictorTest, ReportSameAnchorElementTwice) {
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  uint32_t anchor_id = metrics[0]->anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();
  metrics.clear();

  // Report the same anchor again, it should be ignored.
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->anchor_id = anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  PageAnchorsMetricsObserver::AnchorsData::CreateForWebContents(web_contents());
  PageAnchorsMetricsObserver::AnchorsData* data =
      PageAnchorsMetricsObserver::AnchorsData::FromWebContents(web_contents());
  EXPECT_EQ(1u, data->number_of_anchors_);
}

// Basic test to check the ReportNewAnchorElements method can be
// called with multiple anchors at once.
TEST_F(NavigationPredictorTest, ReportNewAnchorElementsMultipleAnchors) {
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->ratio_distance_top_to_visible_top = 10;
  metrics.push_back(CreateMetricsPtr());
  metrics[1]->contains_image = true;
  metrics[1]->viewport_size = GetDefaultViewport();
  predictor_service()->ReportNewAnchorElements(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  PageAnchorsMetricsObserver::AnchorsData::CreateForWebContents(web_contents());
  PageAnchorsMetricsObserver::AnchorsData* data =
      PageAnchorsMetricsObserver::AnchorsData::FromWebContents(web_contents());
  EXPECT_EQ(2u, data->number_of_anchors_);
  EXPECT_EQ(1u, data->number_of_anchors_contains_image_);
  EXPECT_EQ(0u, data->number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data->number_of_anchors_same_host_);
  EXPECT_EQ(0u, data->number_of_anchors_url_incremented_);
  EXPECT_EQ(20, data->total_clickable_space_);
  EXPECT_EQ(5 * 100, data->MedianLinkLocation());
  EXPECT_EQ(GetDefaultViewport().height(), data->viewport_height_);
  EXPECT_EQ(GetDefaultViewport().width(), data->viewport_width_);
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewport) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->contains_image = true;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->is_in_iframe = true;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->is_url_incremented_by_one = true;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->is_same_host = true;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->ratio_distance_root_top = 0.21;

  int anchor_id_0 = metrics[0]->anchor_id;
  int anchor_id_1 = metrics[1]->anchor_id;
  int anchor_id_2 = metrics[2]->anchor_id;
  int anchor_id_3 = metrics[3]->anchor_id;
  int anchor_id_4 = metrics[4]->anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));

  std::vector<blink::mojom::AnchorElementEnteredViewportPtr> entered_viewport;
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[0]->anchor_id = anchor_id_0;
  predictor_service()->ReportAnchorElementsEnteredViewport(
      std::move(entered_viewport));
  base::RunLoop().RunUntilIdle();

  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  auto get_metric = [&](auto name) {
    return *ukm_recorder.GetEntryMetric(entry, name);
  };
  EXPECT_EQ(0, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(1, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(0, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(0, get_metric(UkmEntry::kPercentVerticalDistanceName));

  entered_viewport.clear();
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[0]->anchor_id = anchor_id_1;
  predictor_service()->ReportAnchorElementsEnteredViewport(
      std::move(entered_viewport));
  base::RunLoop().RunUntilIdle();
  entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  entry = entries[1];
  EXPECT_EQ(1, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(0, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(1, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(0, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(0, get_metric(UkmEntry::kPercentVerticalDistanceName));

  entered_viewport.clear();
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[0]->anchor_id = anchor_id_2;
  predictor_service()->ReportAnchorElementsEnteredViewport(
      std::move(entered_viewport));
  base::RunLoop().RunUntilIdle();
  entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, entries.size());
  entry = entries[2];
  EXPECT_EQ(2, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(0, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(1, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(0, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(0, get_metric(UkmEntry::kPercentVerticalDistanceName));

  entered_viewport.clear();
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[0]->anchor_id = anchor_id_3;
  predictor_service()->ReportAnchorElementsEnteredViewport(
      std::move(entered_viewport));
  base::RunLoop().RunUntilIdle();
  entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(4u, entries.size());
  entry = entries[3];
  EXPECT_EQ(3, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(0, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(1, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(0, get_metric(UkmEntry::kPercentVerticalDistanceName));

  entered_viewport.clear();
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[0]->anchor_id = anchor_id_4;
  predictor_service()->ReportAnchorElementsEnteredViewport(
      std::move(entered_viewport));
  base::RunLoop().RunUntilIdle();
  entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(5u, entries.size());
  entry = entries[4];
  EXPECT_EQ(4, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(0, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(0, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(20, get_metric(UkmEntry::kPercentVerticalDistanceName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportMultiple) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->contains_image = true;
  metrics.push_back(CreateMetricsPtr());
  metrics.back()->is_in_iframe = true;

  int anchor_id_0 = metrics[0]->anchor_id;
  int anchor_id_1 = metrics[1]->anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));

  std::vector<blink::mojom::AnchorElementEnteredViewportPtr> entered_viewport;
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[0]->anchor_id = anchor_id_0;
  entered_viewport.push_back(blink::mojom::AnchorElementEnteredViewport::New());
  entered_viewport[1]->anchor_id = anchor_id_1;
  predictor_service()->ReportAnchorElementsEnteredViewport(
      std::move(entered_viewport));
  base::RunLoop().RunUntilIdle();

  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  auto* entry = entries[0];
  auto get_metric = [&](auto name) {
    return *ukm_recorder.GetEntryMetric(entry, name);
  };
  EXPECT_EQ(0, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(1, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(0, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(0, get_metric(UkmEntry::kPercentVerticalDistanceName));

  entry = entries[1];
  EXPECT_EQ(1, get_metric(UkmEntry::kAnchorIndexName));
  EXPECT_EQ(0, get_metric(UkmEntry::kContainsImageName));
  EXPECT_EQ(1, get_metric(UkmEntry::kIsInIframeName));
  EXPECT_EQ(0, get_metric(UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(0, get_metric(UkmEntry::kSameOriginName));
  EXPECT_EQ(10, get_metric(UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(0, get_metric(UkmEntry::kPercentVerticalDistanceName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementClick) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.push_back(CreateMetricsPtr());

  int anchor_id_0 = metrics[0]->anchor_id;
  GURL target_url = metrics[0]->target_url;
  int anchor_id_1 = metrics[1]->anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));

  auto click = blink::mojom::AnchorElementClick::New();
  click->anchor_id = anchor_id_0;
  click->target_url = target_url;
  predictor_service()->ReportAnchorElementClick(std::move(click));
  base::RunLoop().RunUntilIdle();

  using UkmEntry = ukm::builders::NavigationPredictorPageLinkClick;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  auto get_metric = [&](auto name) {
    return *ukm_recorder.GetEntryMetric(entry, name);
  };
  EXPECT_EQ(0, get_metric(UkmEntry::kAnchorElementIndexName));
  EXPECT_EQ(1, get_metric(UkmEntry::kHrefUnchangedName));

  click = blink::mojom::AnchorElementClick::New();
  click->anchor_id = anchor_id_1;
  // Pretend the page changed the URL since we first saw it.
  click->target_url = GURL("https://changed.com");
  predictor_service()->ReportAnchorElementClick(std::move(click));
  base::RunLoop().RunUntilIdle();
  entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  entry = entries[1];
  EXPECT_EQ(1, get_metric(UkmEntry::kAnchorElementIndexName));
  EXPECT_EQ(0, get_metric(UkmEntry::kHrefUnchangedName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementClickMoreThan10Clicks) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());

  int anchor_id = metrics[0]->anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));

  auto add_click = [&]() {
    auto click = blink::mojom::AnchorElementClick::New();
    click->anchor_id = anchor_id;
    predictor_service()->ReportAnchorElementClick(std::move(click));
    base::RunLoop().RunUntilIdle();
  };

  using UkmEntry = ukm::builders::NavigationPredictorPageLinkClick;
  for (size_t i = 1; i <= 10; i++) {
    add_click();
    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(i, entries.size());
  }
  // Don't log more than 10 clicks.
  for (size_t i = 1; i <= 10; i++) {
    add_click();
    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(10u, entries.size());
  }
}
