// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
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
    NavigationPredictor::Create(
        main_rfh(), predictor_service_.BindNewPipeAndPassReceiver());
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

class MetricsBuilder {
 public:
  explicit MetricsBuilder(NavigationPredictorTest* tester) : tester_(tester) {}

  void AddElementsEnteredViewport(size_t num_elems) {
    for (size_t i = 0; i < num_elems; i++) {
      metrics_.push_back(tester_->CreateMetricsPtr());
      entered_viewport_.push_back(
          blink::mojom::AnchorElementEnteredViewport::New());
      entered_viewport_.back()->anchor_id = metrics_.back()->anchor_id;
    }
  }

  void Run() {
    size_t num_entered_viewport = entered_viewport_.size();
    tester_->predictor_service()->ReportNewAnchorElements(std::move(metrics_));
    tester_->predictor_service()->ReportAnchorElementsEnteredViewport(
        std::move(entered_viewport_));
    metrics_.clear();
    entered_viewport_.clear();
    base::RunLoop().RunUntilIdle();

    using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
    ukm_entries_ = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(num_entered_viewport, ukm_entries_.size());
  }

  uint64_t Entry(size_t idx, const char* name) {
    return *ukm_recorder_.GetEntryMetric(ukm_entries_[idx], name);
  }

  blink::mojom::AnchorElementMetricsPtr& Metrics(size_t index) {
    return metrics_[index];
  }

 private:
  raw_ptr<NavigationPredictorTest> tester_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_;
  std::vector<blink::mojom::AnchorElementEnteredViewportPtr> entered_viewport_;
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries_;
};

TEST_F(NavigationPredictorTest,
       ReportAnchorElementsEnteredViewportContainsImage) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(2);
  builder.Metrics(0)->contains_image = false;
  builder.Metrics(1)->contains_image = true;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kContainsImageName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kContainsImageName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportIsInIframe) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(2);
  builder.Metrics(0)->is_in_iframe = false;
  builder.Metrics(1)->is_in_iframe = true;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kIsInIframeName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kIsInIframeName));
}

TEST_F(NavigationPredictorTest,
       ReportAnchorElementsEnteredViewportIsURLIncrementedByOne) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(2);
  builder.Metrics(0)->is_url_incremented_by_one = false;
  builder.Metrics(1)->is_url_incremented_by_one = true;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kIsURLIncrementedByOneName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kIsURLIncrementedByOneName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportSameOrigin) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(2);
  builder.Metrics(0)->is_same_host = false;
  builder.Metrics(1)->is_same_host = true;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kSameOriginName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kSameOriginName));
}

TEST_F(NavigationPredictorTest,
       ReportAnchorElementsEnteredViewportRatioDistanceRootTop) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(1);
  builder.Metrics(0)->ratio_distance_root_top = 0.21;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(10u, builder.Entry(0, UkmEntry::kPercentClickableAreaName));
  EXPECT_EQ(20u, builder.Entry(0, UkmEntry::kPercentVerticalDistanceName));
}

TEST_F(NavigationPredictorTest,
       ReportAnchorElementsEnteredViewportHasTextSibling) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(2);
  builder.Metrics(0)->has_text_sibling = false;
  builder.Metrics(1)->has_text_sibling = true;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kHasTextSiblingName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kHasTextSiblingName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportFontSize) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(3);
  builder.Metrics(0)->font_size_px = 4;
  builder.Metrics(1)->font_size_px = 12;
  builder.Metrics(2)->font_size_px = 20;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(1u, builder.Entry(0, UkmEntry::kFontSizeName));
  EXPECT_EQ(2u, builder.Entry(1, UkmEntry::kFontSizeName));
  EXPECT_EQ(3u, builder.Entry(2, UkmEntry::kFontSizeName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportIsBold) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(2);
  builder.Metrics(0)->font_weight = 500;
  builder.Metrics(1)->font_weight = 501;
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kIsBoldName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kIsBoldName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportPathLength) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(6);
  builder.Metrics(0)->target_url = GURL("https://foo.com/");
  builder.Metrics(1)->target_url = GURL("https://foo.com/2");
  builder.Metrics(2)->target_url = GURL("https://foo.com/10chars__");
  builder.Metrics(3)->target_url = GURL("https://foo.com/20chars____________");
  builder.Metrics(4)->target_url = GURL("https://foo.com/21chars_____________");
  builder.Metrics(5)->target_url = GURL(
      "https://foo.com/"
      "120chars________________________________________________________________"
      "_______________________________________________");
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(0u, builder.Entry(0, UkmEntry::kPathLengthName));
  EXPECT_EQ(0u, builder.Entry(1, UkmEntry::kPathLengthName));
  EXPECT_EQ(10u, builder.Entry(2, UkmEntry::kPathLengthName));
  EXPECT_EQ(20u, builder.Entry(3, UkmEntry::kPathLengthName));
  EXPECT_EQ(20u, builder.Entry(4, UkmEntry::kPathLengthName));
  EXPECT_EQ(100u, builder.Entry(5, UkmEntry::kPathLengthName));
}

TEST_F(NavigationPredictorTest, ReportAnchorElementsEnteredViewportPathDepth) {
  MetricsBuilder builder(this);
  builder.AddElementsEnteredViewport(5);
  builder.Metrics(0)->target_url = GURL("https://foo.com/");
  builder.Metrics(1)->target_url = GURL("https://foo.com/1");
  builder.Metrics(2)->target_url = GURL("https://foo.com/2/");
  builder.Metrics(3)->target_url = GURL("https://foo.com/1/2/3/4/5");
  builder.Metrics(4)->target_url = GURL("https://foo.com/1/2/3/4/5/6");
  builder.Run();
  using UkmEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  EXPECT_EQ(1u, builder.Entry(0, UkmEntry::kPathDepthName));
  EXPECT_EQ(1u, builder.Entry(1, UkmEntry::kPathDepthName));
  EXPECT_EQ(2u, builder.Entry(2, UkmEntry::kPathDepthName));
  EXPECT_EQ(5u, builder.Entry(3, UkmEntry::kPathDepthName));
  EXPECT_EQ(5u, builder.Entry(4, UkmEntry::kPathDepthName));
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
