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
#include "services/metrics/public/cpp/metrics_utils.h"
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
  const long navigation_start_to_click_ms = 333;
  click->anchor_id = anchor_id_0;
  click->target_url = target_url;
  click->navigation_start_to_click =
      base::Milliseconds(navigation_start_to_click_ms);
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
  EXPECT_EQ(ukm::GetExponentialBucketMin(navigation_start_to_click_ms, 1.3),
            get_metric(UkmEntry::kNavigationStartToLinkClickedMsName));

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

class MockNavigationPredictorForTesting : public NavigationPredictor {
 public:
  using AnchorId = NavigationPredictor::AnchorId;
  static MockNavigationPredictorForTesting* Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementMetricsHost> receiver) {
    // The object is bound to the lifetime of the |render_frame_host| and the
    // mojo connection. See DocumentService for details.
    return new MockNavigationPredictorForTesting(*render_frame_host,
                                                 std::move(receiver));
  }
  void RecordUserInteractionMetrics() {
    auto& user_interactions = GetUserInteractionsData();
    user_interactions.RecordUserInteractionMetrics(ukm_source_id_, {});
  }
  const std::unordered_map<
      int,
      PageAnchorsMetricsObserver::UserInteractionsData::UserInteractions>&
  user_interactions() const {
    return GetUserInteractionsData().user_interactions_;
  }
  const PageAnchorsMetricsObserver::UserInteractionsData::UserInteractions&
  user_interaction(AnchorId anchor_id) {
    auto index_it = tracked_anchor_id_to_index_.find(anchor_id);
    return GetUserInteractionsData().user_interactions_[index_it->second];
  }
  absl::optional<base::TimeDelta> navigation_start_to_click() {
    return navigation_start_to_click_;
  }

 private:
  MockNavigationPredictorForTesting(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementMetricsHost> receiver)
      : NavigationPredictor(render_frame_host, std::move(receiver)) {}
};

TEST_F(NavigationPredictorTest, AnchorElementEnteredAndLeftViewport) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto report_anchor_element_left_viewport =
      [&predictor_service](
          const MockNavigationPredictorForTesting::AnchorId& anchor_id,
          const base::TimeDelta& time_in_viewport) {
        std::vector<blink::mojom::AnchorElementLeftViewportPtr> metrics;
        metrics.push_back(blink::mojom::AnchorElementLeftViewport::New(
            static_cast<uint32_t>(anchor_id), time_in_viewport));
        predictor_service->ReportAnchorElementsLeftViewport(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  auto report_anchor_element_entered_viewport =
      [&predictor_service](
          const MockNavigationPredictorForTesting::AnchorId& anchor_id,
          const base::TimeDelta& navigation_start_to_entered_viewport) {
        std::vector<blink::mojom::AnchorElementEnteredViewportPtr> metrics;
        metrics.push_back(blink::mojom::AnchorElementEnteredViewport::New(
            static_cast<uint32_t>(anchor_id),
            navigation_start_to_entered_viewport));
        predictor_service->ReportAnchorElementsEnteredViewport(
            std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  auto report_new_anchor_element = [&predictor_service, this]() {
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
    metrics.push_back(CreateMetricsPtr());

    MockNavigationPredictorForTesting::AnchorId anchor_id(
        metrics[0]->anchor_id);
    predictor_service->ReportNewAnchorElements(std::move(metrics));
    return anchor_id;
  };

  auto const anchor_id = report_new_anchor_element();

  // Anchor element entered the viewport for the first time. Check user
  // interaction data to see if it is registered.
  const auto navigation_start_to_entered_viewport_1 = base::Milliseconds(150);
  report_anchor_element_entered_viewport(
      anchor_id, navigation_start_to_entered_viewport_1);
  ASSERT_EQ(1u, predictor_service_host->user_interactions().size());
  const auto& user_interactions =
      predictor_service_host->user_interaction(anchor_id);
  EXPECT_TRUE(user_interactions.is_in_viewport);
  EXPECT_TRUE(
      user_interactions.last_navigation_start_to_entered_viewport.has_value());
  EXPECT_EQ(navigation_start_to_entered_viewport_1,
            user_interactions.last_navigation_start_to_entered_viewport);

  // Anchor element left the viewport for the first time.
  const auto time_in_viewport_1 = base::Milliseconds(100);
  report_anchor_element_left_viewport(anchor_id, time_in_viewport_1);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_in_viewport);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_entered_viewport.has_value());
  EXPECT_TRUE(user_interactions.max_time_in_viewport.has_value());
  EXPECT_EQ(time_in_viewport_1, user_interactions.max_time_in_viewport);

  // Anchor element entered the viewport for a second time. It should update the
  // existing user interaction data.
  const auto navigation_start_to_entered_viewport_2 = base::Milliseconds(350);
  report_anchor_element_entered_viewport(
      anchor_id, navigation_start_to_entered_viewport_2);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_TRUE(user_interactions.is_in_viewport);
  EXPECT_EQ(navigation_start_to_entered_viewport_2,
            user_interactions.last_navigation_start_to_entered_viewport);

  // Anchor element left the viewport for a second time. It should update the
  // time_in_viewport to max(time_in_viewport_1, time_in_viewport_2).
  const auto time_in_viewport_2 = base::Milliseconds(200);
  report_anchor_element_left_viewport(anchor_id, time_in_viewport_2);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  // max(time_in_viewport_1, time_in_viewport_2) = time_in_viewport_2
  EXPECT_EQ(time_in_viewport_2, user_interactions.max_time_in_viewport);

  // Anchor element left the viewport for the third time. It should not affect
  // the entered_viewport_to_left_viewport.
  const auto time_in_viewport_3 = base::Milliseconds(120);
  report_anchor_element_left_viewport(anchor_id, time_in_viewport_3);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  // max(time_in_viewport_1, time_in_viewport_2, time_in_viewport_3) =
  // time_in_viewport_2
  EXPECT_EQ(time_in_viewport_2, user_interactions.max_time_in_viewport);
}

TEST_F(NavigationPredictorTest, AnchorElementPointerOverAndHover) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto report_pointer_over =
      [&predictor_service](
          const MockNavigationPredictorForTesting::AnchorId& anchor_id,
          const base::TimeDelta& navigation_start_to_pointer_over) {
        predictor_service->ReportAnchorElementPointerOver(
            blink::mojom::AnchorElementPointerOver::New(
                static_cast<uint32_t>(anchor_id),
                navigation_start_to_pointer_over));
        base::RunLoop().RunUntilIdle();
      };

  auto report_pointer_out =
      [&predictor_service](
          const MockNavigationPredictorForTesting::AnchorId& anchor_id,
          const base::TimeDelta& hover_dwell_time) {
        blink::mojom::AnchorElementPointerOutPtr metrics =
            blink::mojom::AnchorElementPointerOut::New(
                static_cast<uint32_t>(anchor_id), hover_dwell_time);
        predictor_service->ReportAnchorElementPointerOut(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  auto report_new_anchor_element = [&predictor_service, this]() {
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
    metrics.push_back(CreateMetricsPtr());

    MockNavigationPredictorForTesting::AnchorId anchor_id(
        metrics[0]->anchor_id);
    predictor_service->ReportNewAnchorElements(std::move(metrics));
    return anchor_id;
  };

  auto const anchor_id = report_new_anchor_element();

  // Pointer started hovering over the anchor element for the first time. Check
  // user interaction data to see if it is registered.
  const auto navigation_start_to_pointer_over_1 = base::Milliseconds(150);
  report_pointer_over(anchor_id, navigation_start_to_pointer_over_1);
  ASSERT_EQ(1u, predictor_service_host->user_interactions().size());
  const auto& user_interactions =
      predictor_service_host->user_interaction(anchor_id);
  EXPECT_TRUE(user_interactions.is_hovered);
  EXPECT_TRUE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_EQ(navigation_start_to_pointer_over_1,
            user_interactions.last_navigation_start_to_pointer_over);

  // Pointer stopped hovering over the anchor element for the first time.
  const auto hover_dwell_time_1 = base::Milliseconds(100);
  report_pointer_out(anchor_id, hover_dwell_time_1);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_hovered);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_TRUE(user_interactions.max_hover_dwell_time.has_value());
  EXPECT_EQ(hover_dwell_time_1, user_interactions.max_hover_dwell_time);

  // Pointer started hovering over the anchor element for a second time. It
  // should update the existing user interaction data.
  const auto navigation_start_to_pointer_over_2 = base::Milliseconds(450);
  report_pointer_over(anchor_id, navigation_start_to_pointer_over_2);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_TRUE(user_interactions.is_hovered);
  EXPECT_TRUE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_EQ(navigation_start_to_pointer_over_2,
            user_interactions.last_navigation_start_to_pointer_over);

  // Pointer stopped hovering over the anchor element for a second time. It
  // should update the max_hover_dwell_time to max(hover_dwell_time_1,
  // hover_dwell_time_2).
  const auto hover_dwell_time_2 = base::Milliseconds(200);
  report_pointer_out(anchor_id, hover_dwell_time_2);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_hovered);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_TRUE(user_interactions.max_hover_dwell_time.has_value());
  // max(hover_dwell_time_1, hover_dwell_time_2) = hover_dwell_time_2
  EXPECT_EQ(hover_dwell_time_2, user_interactions.max_hover_dwell_time);

  // Pointer stopped hovering over the anchor element for a third time. It
  // should not affect the max_hover_dwell_time.
  const auto hover_dwell_time_3 = base::Milliseconds(50);
  report_pointer_out(anchor_id, hover_dwell_time_3);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_hovered);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_TRUE(user_interactions.max_hover_dwell_time.has_value());
  // max((hover_dwell_time_1, hover_dwell_time_2, hover_dwell_time_3) =
  // hover_dwell_time_2
  EXPECT_EQ(hover_dwell_time_2, user_interactions.max_hover_dwell_time);
}

TEST_F(NavigationPredictorTest, NavigationStartToClick) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  EXPECT_FALSE(predictor_service_host->navigation_start_to_click().has_value());

  const auto navigation_start_to_click = base::Milliseconds(200);
  auto click = blink::mojom::AnchorElementClick::New();
  click->anchor_id = 1;
  click->target_url = GURL("https://example.com/test.html");
  click->navigation_start_to_click = navigation_start_to_click;

  predictor_service->ReportAnchorElementClick(std::move(click));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(navigation_start_to_click,
            predictor_service_host->navigation_start_to_click());
}

TEST_F(NavigationPredictorTest, RecordUserInteractionMetrics) {
  using AnchorId = uint32_t;
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto report_anchor_element_left_viewport =
      [&predictor_service](AnchorId anchor_id,
                           const base::TimeDelta& time_in_viewport) {
        std::vector<blink::mojom::AnchorElementLeftViewportPtr> metrics;
        metrics.push_back(blink::mojom::AnchorElementLeftViewport::New(
            anchor_id, time_in_viewport));
        predictor_service->ReportAnchorElementsLeftViewport(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  auto report_anchor_element_entered_viewport =
      [&predictor_service](
          AnchorId anchor_id,
          const base::TimeDelta& navigation_start_to_entered_viewport) {
        std::vector<blink::mojom::AnchorElementEnteredViewportPtr> metrics;
        metrics.push_back(blink::mojom::AnchorElementEnteredViewport::New(
            anchor_id, navigation_start_to_entered_viewport));
        predictor_service->ReportAnchorElementsEnteredViewport(
            std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };
  auto report_anchor_element_pointer_over =
      [&predictor_service](
          AnchorId anchor_id,
          const base::TimeDelta& navigation_start_to_pinter_over) {
        blink::mojom::AnchorElementPointerOverPtr metrics =
            blink::mojom::AnchorElementPointerOver::New(
                anchor_id, navigation_start_to_pinter_over);
        predictor_service->ReportAnchorElementPointerOver(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  auto report_anchor_element_pointer_hover_dwell_time =
      [&predictor_service](AnchorId anchor_id,
                           const base::TimeDelta& hover_dwell_time) {
        blink::mojom::AnchorElementPointerOutPtr metrics =
            blink::mojom::AnchorElementPointerOut::New(anchor_id,
                                                       hover_dwell_time);
        predictor_service->ReportAnchorElementPointerOut(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.push_back(CreateMetricsPtr());

  int anchor_id_0 = metrics[0]->anchor_id;
  int anchor_id_1 = metrics[1]->anchor_id;
  GURL target_url_1 = metrics[1]->target_url;
  predictor_service->ReportNewAnchorElements(std::move(metrics));

  // Both anchors enter the viewport.
  const int navigation_start_to_entered_viewport = 30;
  report_anchor_element_entered_viewport(
      anchor_id_0, base::Milliseconds(navigation_start_to_entered_viewport));
  report_anchor_element_entered_viewport(
      anchor_id_1, base::Milliseconds(navigation_start_to_entered_viewport));

  // Mouse hover over anchor element 0 and moves away.
  const int navigation_start_to_pinter_over_0 = 140;
  const int hover_dwell_time_0 = 60;
  report_anchor_element_pointer_over(
      anchor_id_0, base::Milliseconds(navigation_start_to_pinter_over_0));
  report_anchor_element_pointer_hover_dwell_time(
      anchor_id_0, base::Milliseconds(hover_dwell_time_0));

  // Anchor element 0 leaves the viewport.
  const int time_in_viewport_0 = 250;
  report_anchor_element_left_viewport(anchor_id_0,
                                      base::Milliseconds(time_in_viewport_0));

  // Mouse hover over anchor element 1 and stays there.
  const int navigation_start_to_pinter_over_1 = 280;
  report_anchor_element_pointer_over(
      anchor_id_1, base::Milliseconds(navigation_start_to_pinter_over_1));

  // Mouse clicks on anchor element 1.
  const int navigation_start_to_click_ms = 430;
  auto click = blink::mojom::AnchorElementClick::New();
  click->anchor_id = anchor_id_1;
  click->target_url = target_url_1;
  click->navigation_start_to_click =
      base::Milliseconds(navigation_start_to_click_ms);
  predictor_service->ReportAnchorElementClick(std::move(click));
  base::RunLoop().RunUntilIdle();

  predictor_service_host->RecordUserInteractionMetrics();
  base::RunLoop().RunUntilIdle();

  // Now check the UKM records.
  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(2u, entries.size());
  auto get_metric = [&](auto anchor_id, auto name) {
    return *ukm_recorder.GetEntryMetric(entries[anchor_id], name);
  };
  for (size_t i = 0; i < entries.size(); i++) {
    int anchor_id = get_metric(i, UkmEntry::kAnchorIndexName);
    EXPECT_TRUE(anchor_id == 0 || anchor_id == 1);
    switch (anchor_id) {
      // Anchor element 0.
      case 0:
        EXPECT_EQ(0, get_metric(i, UkmEntry::kIsInViewportName));
        EXPECT_EQ(0, get_metric(i, UkmEntry::kIsPointerHoveringOverName));
        EXPECT_EQ(
            ukm::GetExponentialBucketMin(time_in_viewport_0, 1.3),
            get_metric(i, UkmEntry::kMaxEnteredViewportToLeftViewportMsName));
        EXPECT_EQ(ukm::GetExponentialBucketMin(hover_dwell_time_0, 1.3),
                  get_metric(i, UkmEntry::kMaxHoverDwellTimeMsName));
        EXPECT_EQ(ukm::GetExponentialBucketMin(1, 1.3),
                  get_metric(i, UkmEntry::kPointerHoveringOverCountName));
        break;

      // Anchor element 1.
      case 1:
        EXPECT_EQ(1, get_metric(i, UkmEntry::kAnchorIndexName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kIsInViewportName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kIsPointerHoveringOverName));
        EXPECT_EQ(
            ukm::GetExponentialBucketMin(
                navigation_start_to_click_ms -
                    navigation_start_to_entered_viewport,
                1.3),
            get_metric(i, UkmEntry::kMaxEnteredViewportToLeftViewportMsName));
        EXPECT_EQ(
            ukm::GetExponentialBucketMin(navigation_start_to_click_ms -
                                             navigation_start_to_pinter_over_1,
                                         1.3),
            get_metric(i, UkmEntry::kMaxHoverDwellTimeMsName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kPointerHoveringOverCountName));
        break;
    }
  }
}

TEST_F(NavigationPredictorTest, RecordPreloadingOnHover) {
  using AnchorId = uint32_t;
  auto report_pointer_down =
      [this](AnchorId anchor_id,
             const base::TimeDelta& navigation_start_to_pointer_down) {
        blink::mojom::AnchorElementPointerDownPtr metrics =
            blink::mojom::AnchorElementPointerDown::New(
                anchor_id, navigation_start_to_pointer_down);
        predictor_service()->ReportAnchorElementPointerDown(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };
  auto report_pointer_over =
      [this](AnchorId anchor_id,
             const base::TimeDelta& navigation_start_to_pinter_over) {
        blink::mojom::AnchorElementPointerOverPtr metrics =
            blink::mojom::AnchorElementPointerOver::New(
                anchor_id, navigation_start_to_pinter_over);
        predictor_service()->ReportAnchorElementPointerOver(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };
  auto report_pointer_out = [this](AnchorId anchor_id,
                                   const base::TimeDelta& hover_dwell_time) {
    blink::mojom::AnchorElementPointerOutPtr metrics =
        blink::mojom::AnchorElementPointerOut::New(anchor_id, hover_dwell_time);
    predictor_service()->ReportAnchorElementPointerOut(std::move(metrics));
    base::RunLoop().RunUntilIdle();
  };
  auto report_click = [this](AnchorId anchor_id, const GURL& target_url,
                             const base::TimeDelta& navigation_start_to_click) {
    auto click = blink::mojom::AnchorElementClick::New();
    click->anchor_id = anchor_id;
    click->target_url = target_url;
    click->navigation_start_to_click = navigation_start_to_click;
    predictor_service()->ReportAnchorElementClick(std::move(click));
    base::RunLoop().RunUntilIdle();
  };

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  using UkmEntry = ukm::builders::NavigationPredictorPreloadOnHover;

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.push_back(CreateMetricsPtr());

  int anchor_id_0 = metrics[0]->anchor_id;
  int anchor_id_1 = metrics[1]->anchor_id;
  GURL target_url = metrics[1]->target_url;
  predictor_service()->ReportNewAnchorElements(std::move(metrics));

  // Mouse moves over anchor_id_0, mouse down and then moves away.
  report_pointer_over(
      anchor_id_0, /*navigation_start_to_pinter_over=*/base::Milliseconds(10));
  report_pointer_down(
      anchor_id_0, /*navigation_start_to_pointer_down=*/base::Milliseconds(30));
  report_pointer_out(anchor_id_0, /*hover_dwell_time=*/base::Milliseconds(70));
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto get_metric = [](const auto& entries, auto anchor_id, auto name) {
    return *ukm::TestUkmRecorder::GetEntryMetric(entries[anchor_id], name);
  };
  auto has_metric = [&](const auto& entries, auto anchor_id, auto name) {
    return ukm::TestUkmRecorder::EntryHasMetric(entries[anchor_id], name);
  };
  EXPECT_EQ(ukm::GetExponentialBucketMin(70, 1.3),
            get_metric(entries, 0, "HoverNotTakenMs"));
  EXPECT_EQ(ukm::GetExponentialBucketMin(50, 1.3),
            get_metric(entries, 0, "MouseDownNotTakenMs"));
  EXPECT_FALSE(has_metric(entries, 0, "HoverTakenMs"));
  EXPECT_FALSE(has_metric(entries, 0, "MouseDownTakenMs"));

  // Mouse moves over anchor_id_1, mouse down and then click event happens.
  report_pointer_over(
      anchor_id_1, /*navigation_start_to_pinter_over=*/base::Milliseconds(30));
  report_pointer_down(
      anchor_id_1, /*navigation_start_to_pointer_down=*/base::Milliseconds(60));
  report_click(anchor_id_1, target_url,
               /*navigation_start_to_click=*/base::Milliseconds(90));
  entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(ukm::GetExponentialBucketMin(60, 1.3),
            get_metric(entries, 1, "HoverTakenMs"));
  EXPECT_EQ(ukm::GetExponentialBucketMin(30, 1.3),
            get_metric(entries, 1, "MouseDownTakenMs"));
  EXPECT_FALSE(has_metric(entries, 1, "HoverNotTakenMs"));
  EXPECT_FALSE(has_metric(entries, 1, "MouseDownNotTakenMs"));

  // Pointer down event followed by a pointer out event without any pointer over
  // event should not cause a crash (crbug/1423336).
  report_pointer_down(
      anchor_id_0, /*navigation_start_to_pointer_down=*/base::Milliseconds(10));
  report_pointer_out(anchor_id_0, /*hover_dwell_time=*/base::Milliseconds(20));
}

TEST_F(NavigationPredictorTest,
       UserInteractionMetricsIsClearedAfterNavigation) {
  using AnchorId = uint32_t;
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto report_anchor_element_entered_viewport =
      [&predictor_service](
          AnchorId anchor_id,
          const base::TimeDelta& navigation_start_to_entered_viewport) {
        std::vector<blink::mojom::AnchorElementEnteredViewportPtr> metrics;
        metrics.push_back(blink::mojom::AnchorElementEnteredViewport::New(
            anchor_id, navigation_start_to_entered_viewport));
        predictor_service->ReportAnchorElementsEnteredViewport(
            std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };
  auto report_anchor_element_pointer_over =
      [&predictor_service](
          AnchorId anchor_id,
          const base::TimeDelta& navigation_start_to_pinter_over) {
        blink::mojom::AnchorElementPointerOverPtr metrics =
            blink::mojom::AnchorElementPointerOver::New(
                anchor_id, navigation_start_to_pinter_over);
        predictor_service->ReportAnchorElementPointerOver(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };

  auto report_anchor_element_pointer_hover_dwell_time =
      [&predictor_service](AnchorId anchor_id,
                           const base::TimeDelta& hover_dwell_time) {
        blink::mojom::AnchorElementPointerOutPtr metrics =
            blink::mojom::AnchorElementPointerOut::New(anchor_id,
                                                       hover_dwell_time);
        predictor_service->ReportAnchorElementPointerOut(std::move(metrics));
        base::RunLoop().RunUntilIdle();
      };
  // Add two anchor elements and interact with them.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
    metrics.push_back(CreateMetricsPtr());
    metrics.push_back(CreateMetricsPtr());

    int anchor_id_0 = metrics[0]->anchor_id;
    int anchor_id_1 = metrics[1]->anchor_id;
    predictor_service->ReportNewAnchorElements(std::move(metrics));

    // Both anchors enter the viewport.
    const int navigation_start_to_entered_viewport = 30;
    report_anchor_element_entered_viewport(
        anchor_id_0, base::Milliseconds(navigation_start_to_entered_viewport));
    report_anchor_element_entered_viewport(
        anchor_id_1, base::Milliseconds(navigation_start_to_entered_viewport));

    // Mouse hover over anchor element 0 and moves away.
    const int navigation_start_to_pinter_over_0 = 140;
    const int hover_dwell_time_0 = 60;
    report_anchor_element_pointer_over(
        anchor_id_0, base::Milliseconds(navigation_start_to_pinter_over_0));
    report_anchor_element_pointer_hover_dwell_time(
        anchor_id_0, base::Milliseconds(hover_dwell_time_0));

    predictor_service_host->RecordUserInteractionMetrics();
    base::RunLoop().RunUntilIdle();

    // Now check the UKM records.
    using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(2u, entries.size());
  }

  // This time we only have 1 anchor element.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
    metrics.push_back(CreateMetricsPtr());
    int anchor_id_0 = metrics[0]->anchor_id;
    predictor_service->ReportNewAnchorElements(std::move(metrics));

    // The anchor enter the viewport.
    const int navigation_start_to_entered_viewport = 90;
    report_anchor_element_entered_viewport(
        anchor_id_0, base::Milliseconds(navigation_start_to_entered_viewport));

    // Mouse hover over anchor element 0 and moves away.
    const int navigation_start_to_pinter_over_0 = 200;
    const int hover_dwell_time_0 = 20;  // it is less than 60ms
    report_anchor_element_pointer_over(
        anchor_id_0, base::Milliseconds(navigation_start_to_pinter_over_0));
    report_anchor_element_pointer_hover_dwell_time(
        anchor_id_0, base::Milliseconds(hover_dwell_time_0));

    predictor_service_host->RecordUserInteractionMetrics();
    base::RunLoop().RunUntilIdle();

    using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto get_metric = [&](auto name) {
      return *ukm_recorder.GetEntryMetric(entries[0], name);
    };

    EXPECT_EQ(anchor_id_0, get_metric(UkmEntry::kAnchorIndexName));
    EXPECT_EQ(1, get_metric(UkmEntry::kIsInViewportName));
    EXPECT_EQ(0, get_metric(UkmEntry::kIsPointerHoveringOverName));
    EXPECT_EQ(ukm::GetExponentialBucketMin(hover_dwell_time_0, 1.3),
              get_metric(UkmEntry::kMaxHoverDwellTimeMsName));
    EXPECT_EQ(1, get_metric(UkmEntry::kPointerHoveringOverCountName));
  }
}
