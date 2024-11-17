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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "navigation_predictor_metrics_document_data.h"
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
  NavigationPredictorTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {}
  ~NavigationPredictorTest() override = default;

  // Helper function to generate mojom metrics.
  blink::mojom::AnchorElementMetricsPtr CreateMetricsPtr(
      std::optional<int> anchor_id = std::nullopt) {
    if (anchor_id.has_value()) {
      next_id_ = anchor_id.value();
    }
    auto metrics = blink::mojom::AnchorElementMetrics::New();
    metrics->anchor_id = next_id_++;
    metrics->target_url = GURL("https://google.com");
    metrics->ratio_area = 0.1;
    return metrics;
  }

  gfx::Size GetDefaultViewport() { return gfx::Size(600, 800); }

  blink::mojom::AnchorElementMetricsHost* predictor_service() const {
    return predictor_service_.get();
  }

  void RecordPropertyPageLinkClickDataToUkm() {
    NavigationPredictorMetricsDocumentData* data =
        NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
            main_rfh());
    data->RecordPageLinkClickData(main_rfh()->GetPageUkmSourceId());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetUp() override {
    // To avoid tsan data race test flakes, this needs to happen before
    // ChromeRenderViewHostTestHarness::SetUp causes tasks on other threads
    // to check if a feature is enabled.
    SetupFieldTrial();

    ChromeRenderViewHostTestHarness::SetUp();

    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://example.com"));

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
    params["traffic_client_enabled_percent"] = "100";

    // TODO(crbug.com/40278151): Repeated executions of the model may be
    // undesirable for performance, but that behaviour is what's tested in
    // ProcessPointerEventUsingMLModel. Determine what the final behaviour
    // should be and then update the test accordingly.
    std::map<std::string, std::string> ml_model_params;
    ml_model_params["one_execution_per_hover"] = "false";
    ml_model_params["max_hover_time"] = "10s";

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kNavigationPredictor, params},
         {blink::features::kPreloadingHeuristicsMLModel, ml_model_params}},
        {});
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

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
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();

  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh())
          ->GetAnchorsData();
  EXPECT_EQ(1u, data.number_of_anchors_);
  EXPECT_EQ(0u, data.number_of_anchors_contains_image_);
  EXPECT_EQ(0u, data.number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data.number_of_anchors_same_host_);
  EXPECT_EQ(0u, data.number_of_anchors_url_incremented_);
  EXPECT_EQ(10, data.total_clickable_space_);
  EXPECT_EQ(10 * 100, data.MedianLinkLocation());
  EXPECT_EQ(GetDefaultViewport().height(), data.viewport_height_);
  EXPECT_EQ(GetDefaultViewport().width(), data.viewport_width_);

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->contains_image = true;
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, data.number_of_anchors_);
  EXPECT_EQ(1u, data.number_of_anchors_contains_image_);
  EXPECT_EQ(0u, data.number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data.number_of_anchors_same_host_);
  EXPECT_EQ(0u, data.number_of_anchors_url_incremented_);
  EXPECT_EQ(20, data.total_clickable_space_);
  EXPECT_EQ(5 * 100, data.MedianLinkLocation());

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->is_in_iframe = true;
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, data.number_of_anchors_);
  EXPECT_EQ(1u, data.number_of_anchors_contains_image_);
  EXPECT_EQ(1u, data.number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data.number_of_anchors_same_host_);
  EXPECT_EQ(0u, data.number_of_anchors_url_incremented_);
  EXPECT_EQ(30, data.total_clickable_space_);
  EXPECT_EQ(0, data.MedianLinkLocation());

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->is_same_host = true;
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4u, data.number_of_anchors_);
  EXPECT_EQ(1u, data.number_of_anchors_contains_image_);
  EXPECT_EQ(1u, data.number_of_anchors_in_iframe_);
  EXPECT_EQ(1u, data.number_of_anchors_same_host_);
  EXPECT_EQ(0u, data.number_of_anchors_url_incremented_);
  EXPECT_EQ(40, data.total_clickable_space_);
  EXPECT_EQ(0, data.MedianLinkLocation());

  metrics.clear();
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->is_url_incremented_by_one = true;
  metrics[0]->ratio_area = 0.05;
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, data.number_of_anchors_);
  EXPECT_EQ(1u, data.number_of_anchors_contains_image_);
  EXPECT_EQ(1u, data.number_of_anchors_in_iframe_);
  EXPECT_EQ(1u, data.number_of_anchors_same_host_);
  EXPECT_EQ(1u, data.number_of_anchors_url_incremented_);
  EXPECT_EQ(45, data.total_clickable_space_);
  EXPECT_EQ(0, data.MedianLinkLocation());
}

TEST_F(NavigationPredictorTest, ReportSameAnchorElementTwice) {
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  uint32_t anchor_id = metrics[0]->anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  metrics.clear();

  // Report the same anchor again, it should be ignored.
  metrics.push_back(CreateMetricsPtr());
  metrics[0]->anchor_id = anchor_id;
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();

  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh())
          ->GetAnchorsData();

  EXPECT_EQ(1u, data.number_of_anchors_);
}

TEST_F(NavigationPredictorTest, MedianLinkLocation) {
  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh())
          ->GetAnchorsData();

  // Sets `link_locations_` to some contrived, shuffled values to test the
  // median calculation.

  // Odd number of elements.
  data.link_locations_ = {80, 50, 60, 10, 70, 20, 40, 30, 90};
  EXPECT_EQ(50 * 100, data.MedianLinkLocation());

  // Even number of elements, distinct middle values (50 and 60).
  data.link_locations_ = {40, 10, 50, 30, 70, 100, 90, 20, 80, 60};
  EXPECT_EQ(55 * 100, data.MedianLinkLocation());

  // Even number of elements, middle values (50) are equal.
  data.link_locations_ = {80, 40, 50, 20, 30, 10, 100, 90, 50, 70};
  EXPECT_EQ(50 * 100, data.MedianLinkLocation());
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
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();

  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh())
          ->GetAnchorsData();
  EXPECT_EQ(2u, data.number_of_anchors_);
  EXPECT_EQ(1u, data.number_of_anchors_contains_image_);
  EXPECT_EQ(0u, data.number_of_anchors_in_iframe_);
  EXPECT_EQ(0u, data.number_of_anchors_same_host_);
  EXPECT_EQ(0u, data.number_of_anchors_url_incremented_);
  EXPECT_EQ(20, data.total_clickable_space_);
  EXPECT_EQ(5 * 100, data.MedianLinkLocation());
  EXPECT_EQ(GetDefaultViewport().height(), data.viewport_height_);
  EXPECT_EQ(GetDefaultViewport().width(), data.viewport_width_);
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
    tester_->predictor_service()->ReportNewAnchorElements(
        std::move(metrics_), /*removed_elements=*/{});
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
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      ukm_entries_;
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
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});

  auto click = blink::mojom::AnchorElementClick::New();
  const long navigation_start_to_click_ms = 333;
  click->anchor_id = anchor_id_0;
  click->target_url = target_url;
  click->navigation_start_to_click =
      base::Milliseconds(navigation_start_to_click_ms);
  predictor_service()->ReportAnchorElementClick(std::move(click));
  base::RunLoop().RunUntilIdle();
  RecordPropertyPageLinkClickDataToUkm();

  using UkmEntry = ukm::builders::NavigationPredictorPageLinkClick;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
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
  RecordPropertyPageLinkClickDataToUkm();
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
  predictor_service()->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});

  auto add_click = [&]() {
    auto click = blink::mojom::AnchorElementClick::New();
    click->anchor_id = anchor_id;
    predictor_service()->ReportAnchorElementClick(std::move(click));
    base::RunLoop().RunUntilIdle();
  };

  using UkmEntry = ukm::builders::NavigationPredictorPageLinkClick;
  for (size_t i = 1; i <= 10; i++) {
    add_click();
    RecordPropertyPageLinkClickDataToUkm();
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
    auto& data = GetNavigationPredictorMetricsDocumentData();
    data.RecordUserInteractionsData(ukm_source_id_);
  }
  void RecordPreloadOnHoverMetrics() {
    auto& data = GetNavigationPredictorMetricsDocumentData();
    data.RecordPreloadOnHoverData(ukm_source_id_);
  }
  std::unordered_map<
      int,
      NavigationPredictorMetricsDocumentData::UserInteractionsData>&
  user_interactions() {
    return GetNavigationPredictorMetricsDocumentData()
        .GetUserInteractionsData();
  }
  const NavigationPredictorMetricsDocumentData::UserInteractionsData&
  user_interaction(AnchorId anchor_id) {
    auto index_it = tracked_anchor_id_to_index_.find(anchor_id);
    return user_interactions()[index_it->second];
  }
  std::optional<base::TimeDelta> navigation_start_to_click() {
    return navigation_start_to_click_;
  }
  int GetAnchorIndex(AnchorId anchor_id) {
    auto it = tracked_anchor_id_to_index_.find(anchor_id);
    return (it != tracked_anchor_id_to_index_.end()) ? it->second : -1;
  }
  size_t NumAnchorElementData() const { return anchors_.size(); }
  // NavigationPredictor::
  void OnPreloadingHeuristicsModelDone(
      GURL url,
      PreloadingModelKeyedService::Result result) override {
    NavigationPredictor::OnPreloadingHeuristicsModelDone(url, result);
    if (on_preloading_heuristics_mode_done_callback_) {
      std::move(on_preloading_heuristics_mode_done_callback_).Run(result);
    }
  }

  void SetOnPreloadingHeuristicsModelDoneCallback(
      base::OnceCallback<void(PreloadingModelKeyedService::Result)> callback) {
    on_preloading_heuristics_mode_done_callback_ = std::move(callback);
  }

 private:
  MockNavigationPredictorForTesting(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementMetricsHost> receiver)
      : NavigationPredictor(render_frame_host, std::move(receiver)) {}
  base::OnceCallback<void(PreloadingModelKeyedService::Result)>
      on_preloading_heuristics_mode_done_callback_;
};

class NavigationPredictorUserInteractionsTest : public NavigationPredictorTest {
 public:
  NavigationPredictorUserInteractionsTest() = default;
  ~NavigationPredictorUserInteractionsTest() override = default;

  MockNavigationPredictorForTesting::AnchorId ReportNewAnchorElement(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      std::optional<int> id = std::nullopt) {
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
    metrics.push_back(CreateMetricsPtr(id));

    MockNavigationPredictorForTesting::AnchorId anchor_id(
        metrics[0]->anchor_id);
    predictor_service->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
    return anchor_id;
  }

  MockNavigationPredictorForTesting::AnchorId ReportNewAnchorElementWithDetails(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      float ratio_area,
      float ratio_distance_top_to_visible_top,
      float ratio_distance_root_top,
      bool is_in_iframe,
      bool contains_image,
      bool is_same_host,
      bool is_url_incremented_by_one,
      bool has_text_sibling,
      uint32_t font_size_px,
      uint32_t font_weight) {
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
    metrics.push_back(CreateMetricsPtr());

    metrics[0]->ratio_area = ratio_area;
    metrics[0]->ratio_distance_top_to_visible_top =
        ratio_distance_top_to_visible_top;
    metrics[0]->ratio_distance_root_top = ratio_distance_root_top;
    metrics[0]->is_in_iframe = is_in_iframe;
    metrics[0]->contains_image = contains_image;
    metrics[0]->is_same_host = is_same_host;
    metrics[0]->is_url_incremented_by_one = is_url_incremented_by_one;
    metrics[0]->has_text_sibling = has_text_sibling;
    metrics[0]->font_size_px = font_size_px;
    metrics[0]->font_weight = font_weight;

    MockNavigationPredictorForTesting::AnchorId anchor_id(
        metrics[0]->anchor_id);
    predictor_service->ReportNewAnchorElements(std::move(metrics),
                                               /*removed_elements=*/{});
    return anchor_id;
  }

  void ReportAnchorElementLeftViewport(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      const base::TimeDelta& time_in_viewport) {
    std::vector<blink::mojom::AnchorElementLeftViewportPtr> metrics;
    metrics.push_back(blink::mojom::AnchorElementLeftViewport::New(
        static_cast<uint32_t>(anchor_id), time_in_viewport));
    predictor_service->ReportAnchorElementsLeftViewport(std::move(metrics));
    base::RunLoop().RunUntilIdle();
  }

  void ReportAnchorElementEnteredViewport(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      const base::TimeDelta& navigation_start_to_entered_viewport) {
    std::vector<blink::mojom::AnchorElementEnteredViewportPtr> metrics;
    metrics.push_back(blink::mojom::AnchorElementEnteredViewport::New(
        static_cast<uint32_t>(anchor_id),
        navigation_start_to_entered_viewport));
    predictor_service->ReportAnchorElementsEnteredViewport(std::move(metrics));
    base::RunLoop().RunUntilIdle();
  }

  void ReportAnchorElementPositionUpdate(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      float vertical_position_ratio,
      std::optional<float> distance_from_pointer_down_ratio) {
    std::vector<blink::mojom::AnchorElementPositionUpdatePtr> metrics;
    metrics.push_back(blink::mojom::AnchorElementPositionUpdate::New(
        static_cast<uint32_t>(anchor_id), vertical_position_ratio,
        distance_from_pointer_down_ratio));
    predictor_service->ReportAnchorElementsPositionUpdate(std::move(metrics));
    base::RunLoop().RunUntilIdle();
  }

  void ReportAnchorElementPointerOver(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      const base::TimeDelta& navigation_start_to_pointer_over) {
    predictor_service->ReportAnchorElementPointerOver(
        blink::mojom::AnchorElementPointerOver::New(
            static_cast<uint32_t>(anchor_id),
            navigation_start_to_pointer_over));
    base::RunLoop().RunUntilIdle();
  }

  void ReportAnchorElementPointerOut(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      const base::TimeDelta& hover_dwell_time) {
    blink::mojom::AnchorElementPointerOutPtr metrics =
        blink::mojom::AnchorElementPointerOut::New(
            static_cast<uint32_t>(anchor_id), hover_dwell_time);
    predictor_service->ReportAnchorElementPointerOut(std::move(metrics));
    base::RunLoop().RunUntilIdle();
  }
  void ReportAnchorElementPointerDataOnHoverTimerFired(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      double mouse_velocity,
      double mouse_acceleration) {
    blink::mojom::AnchorElementPointerDataOnHoverTimerFiredPtr metrics =
        blink::mojom::AnchorElementPointerDataOnHoverTimerFired::New(
            static_cast<uint32_t>(anchor_id),
            blink::mojom::AnchorElementPointerData::New(
                /*is_mouse_pointer=*/true, mouse_velocity, mouse_acceleration));
    predictor_service->ReportAnchorElementPointerDataOnHoverTimerFired(
        std::move(metrics));
    base::RunLoop().RunUntilIdle();
  }

  void ReportAnchorElementClick(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      const GURL& target_url,
      base::TimeDelta navigation_start_to_click) {
    auto click = blink::mojom::AnchorElementClick::New();
    click->anchor_id = static_cast<uint32_t>(anchor_id);
    click->target_url = target_url;
    click->navigation_start_to_click = navigation_start_to_click;
    predictor_service->ReportAnchorElementClick(std::move(click));
    base::RunLoop().RunUntilIdle();
  }

  void ReportAnchorElementPointerDown(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      const base::TimeDelta& navigation_start_to_pointer_down) {
    blink::mojom::AnchorElementPointerDownPtr metrics =
        blink::mojom::AnchorElementPointerDown::New(
            static_cast<uint32_t>(anchor_id), navigation_start_to_pointer_down);
    predictor_service->ReportAnchorElementPointerDown(std::move(metrics));
    base::RunLoop().RunUntilIdle();
  }

  void ProcessPointerEventUsingMLModel(
      blink::mojom::AnchorElementMetricsHost* predictor_service,
      MockNavigationPredictorForTesting::AnchorId anchor_id,
      bool is_mouse,
      blink::mojom::AnchorElementUserInteractionEventForMLModelType
          user_interaction_event_type) {
    blink::mojom::AnchorElementPointerEventForMLModelPtr pointer_event =
        blink::mojom::AnchorElementPointerEventForMLModel::New(
            /*anchor_id=*/static_cast<uint32_t>(anchor_id),
            /*is_mouse=*/is_mouse,
            /*user_interaction_event_type=*/user_interaction_event_type);
    predictor_service->ProcessPointerEventUsingMLModel(
        std::move(pointer_event));
    base::RunLoop().RunUntilIdle();
  }
};

TEST_F(NavigationPredictorUserInteractionsTest,
       AnchorElementEnteredAndLeftViewport) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto anchor_id = ReportNewAnchorElement(predictor_service.get());

  // Anchor element entered the viewport for the first time. Check user
  // interaction data to see if it is registered.
  const auto navigation_start_to_entered_viewport_1 = base::Milliseconds(150);
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id,
                                     navigation_start_to_entered_viewport_1);
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
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id,
                                  time_in_viewport_1);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_in_viewport);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_entered_viewport.has_value());
  EXPECT_TRUE(user_interactions.max_time_in_viewport.has_value());
  EXPECT_EQ(time_in_viewport_1, user_interactions.max_time_in_viewport);

  // Anchor element entered the viewport for a second time. It should update the
  // existing user interaction data.
  const auto navigation_start_to_entered_viewport_2 = base::Milliseconds(350);
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id,
                                     navigation_start_to_entered_viewport_2);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_TRUE(user_interactions.is_in_viewport);
  EXPECT_EQ(navigation_start_to_entered_viewport_2,
            user_interactions.last_navigation_start_to_entered_viewport);

  // Anchor element left the viewport for a second time. It should update the
  // time_in_viewport to max(time_in_viewport_1, time_in_viewport_2).
  const auto time_in_viewport_2 = base::Milliseconds(200);
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id,
                                  time_in_viewport_2);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  // max(time_in_viewport_1, time_in_viewport_2) = time_in_viewport_2
  EXPECT_EQ(time_in_viewport_2, user_interactions.max_time_in_viewport);

  // Anchor element left the viewport for the third time. It should not affect
  // the entered_viewport_to_left_viewport.
  const auto time_in_viewport_3 = base::Milliseconds(120);
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id,
                                  time_in_viewport_3);
  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  // max(time_in_viewport_1, time_in_viewport_2, time_in_viewport_3) =
  // time_in_viewport_2
  EXPECT_EQ(time_in_viewport_2, user_interactions.max_time_in_viewport);
}

TEST_F(NavigationPredictorUserInteractionsTest,
       AnchorElementPointerOverAndHover) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto anchor_id = ReportNewAnchorElement(predictor_service.get());

  // Pointer started hovering over the anchor element for the first time. Check
  // user interaction data to see if it is registered.
  const auto navigation_start_to_pointer_over_1 = base::Milliseconds(150);
  ReportAnchorElementPointerOver(predictor_service.get(), anchor_id,
                                 navigation_start_to_pointer_over_1);
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
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id,
                                hover_dwell_time_1);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_hovered);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_TRUE(user_interactions.max_hover_dwell_time.has_value());
  EXPECT_EQ(hover_dwell_time_1, user_interactions.max_hover_dwell_time);

  // Pointer started hovering over the anchor element for a second time. It
  // should update the existing user interaction data.
  const auto navigation_start_to_pointer_over_2 = base::Milliseconds(450);
  ReportAnchorElementPointerOver(predictor_service.get(), anchor_id,
                                 navigation_start_to_pointer_over_2);
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
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id,
                                hover_dwell_time_2);

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
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id,
                                hover_dwell_time_3);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  EXPECT_FALSE(user_interactions.is_hovered);
  EXPECT_FALSE(
      user_interactions.last_navigation_start_to_pointer_over.has_value());
  EXPECT_TRUE(user_interactions.max_hover_dwell_time.has_value());
  // max((hover_dwell_time_1, hover_dwell_time_2, hover_dwell_time_3) =
  // hover_dwell_time_2
  EXPECT_EQ(hover_dwell_time_2, user_interactions.max_hover_dwell_time);
}

TEST_F(NavigationPredictorUserInteractionsTest, NavigationStartToClick) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  EXPECT_FALSE(predictor_service_host->navigation_start_to_click().has_value());

  const auto navigation_start_to_click = base::Milliseconds(200);
  auto anchor_id = MockNavigationPredictorForTesting::AnchorId(1);
  ReportAnchorElementClick(predictor_service.get(), anchor_id,
                           GURL("https://example.com/test.html"),
                           navigation_start_to_click);
  EXPECT_EQ(navigation_start_to_click,
            predictor_service_host->navigation_start_to_click());
}

TEST_F(NavigationPredictorUserInteractionsTest, RecordUserInteractionMetrics) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.push_back(CreateMetricsPtr());

  auto anchor_id_0 =
      MockNavigationPredictorForTesting::AnchorId(metrics[0]->anchor_id);
  auto anchor_id_1 =
      MockNavigationPredictorForTesting::AnchorId(metrics[1]->anchor_id);
  GURL target_url_1 = metrics[1]->target_url;
  predictor_service->ReportNewAnchorElements(std::move(metrics),
                                             /*removed_elements=*/{});

  // Both anchors enter the viewport.
  const int navigation_start_to_entered_viewport = 30;
  ReportAnchorElementEnteredViewport(
      predictor_service.get(), anchor_id_0,
      base::Milliseconds(navigation_start_to_entered_viewport));
  ReportAnchorElementEnteredViewport(
      predictor_service.get(), anchor_id_1,
      base::Milliseconds(navigation_start_to_entered_viewport));

  // Mouse hover over anchor element 0 and moves away.
  const int navigation_start_to_pointer_over_0 = 140;
  const int hover_dwell_time_0 = 60;
  const double mouse_velocity = 50.0;
  const double mouse_acceleration = -10.0;
  ReportAnchorElementPointerOver(
      predictor_service.get(), anchor_id_0,
      base::Milliseconds(navigation_start_to_pointer_over_0));
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id_0,
                                base::Milliseconds(hover_dwell_time_0));
  ReportAnchorElementPointerDataOnHoverTimerFired(
      predictor_service.get(), anchor_id_0, mouse_velocity, mouse_acceleration);

  // Anchor element 0 leaves the viewport.
  const int time_in_viewport_0 = 250;
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id_0,
                                  base::Milliseconds(time_in_viewport_0));

  // Anchor element 0 enters and leaves the viewport again.
  ReportAnchorElementEnteredViewport(
      predictor_service.get(), anchor_id_0,
      base::Milliseconds(navigation_start_to_entered_viewport +
                         time_in_viewport_0 + 1));
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id_0,
                                  base::Milliseconds(1));

  // Mouse hover over anchor element 1 and stays there.
  const int navigation_start_to_pointer_over_1 = 280;
  ReportAnchorElementPointerOver(
      predictor_service.get(), anchor_id_1,
      base::Milliseconds(navigation_start_to_pointer_over_1));

  // Mouse clicks on anchor element 1.
  const int navigation_start_to_click_ms = 430;
  ReportAnchorElementClick(predictor_service.get(), anchor_id_1, target_url_1,
                           base::Milliseconds(navigation_start_to_click_ms));

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
        EXPECT_EQ(2, get_metric(i, UkmEntry::kEnteredViewportCountName));
        EXPECT_EQ(0, get_metric(i, UkmEntry::kIsPointerHoveringOverName));
        EXPECT_EQ(
            ukm::GetExponentialBucketMin(time_in_viewport_0, 1.3),
            get_metric(i, UkmEntry::kMaxEnteredViewportToLeftViewportMsName));
        EXPECT_EQ(ukm::GetExponentialBucketMin(hover_dwell_time_0, 1.3),
                  get_metric(i, UkmEntry::kMaxHoverDwellTimeMsName));
        EXPECT_EQ(ukm::GetExponentialBucketMin(1, 1.3),
                  get_metric(i, UkmEntry::kPointerHoveringOverCountName));
        EXPECT_EQ(/*get_exponential_bucket_for_signed_values(50)=*/40,
                  get_metric(i, UkmEntry::kMouseVelocityName));
        EXPECT_EQ(/*get_exponential_bucket_for_signed_values(-10)=*/-9,
                  get_metric(i, UkmEntry::kMouseAccelerationName));
        break;

      // Anchor element 1.
      case 1:
        EXPECT_EQ(1, get_metric(i, UkmEntry::kAnchorIndexName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kIsInViewportName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kEnteredViewportCountName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kIsPointerHoveringOverName));
        EXPECT_EQ(
            ukm::GetExponentialBucketMin(
                navigation_start_to_click_ms -
                    navigation_start_to_entered_viewport,
                1.3),
            get_metric(i, UkmEntry::kMaxEnteredViewportToLeftViewportMsName));
        EXPECT_EQ(
            ukm::GetExponentialBucketMin(navigation_start_to_click_ms -
                                             navigation_start_to_pointer_over_1,
                                         1.3),
            get_metric(i, UkmEntry::kMaxHoverDwellTimeMsName));
        EXPECT_EQ(1, get_metric(i, UkmEntry::kPointerHoveringOverCountName));
        break;
    }
  }
}

TEST_F(NavigationPredictorUserInteractionsTest, RecordPreloadingOnHover) {
  using AnchorId = MockNavigationPredictorForTesting::AnchorId;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  using UkmEntry = ukm::builders::NavigationPredictorPreloadOnHover;

  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr());
  metrics.push_back(CreateMetricsPtr());

  AnchorId anchor_id_0(metrics[0]->anchor_id);
  AnchorId anchor_id_1(metrics[1]->anchor_id);
  GURL target_url = metrics[1]->target_url;
  predictor_service->ReportNewAnchorElements(std::move(metrics),
                                             /*removed_elements=*/{});

  // Mouse moves over anchor_id_0, mouse down and then moves away.
  ReportAnchorElementPointerOver(
      predictor_service.get(), anchor_id_0,
      /*navigation_start_to_pointer_over=*/base::Milliseconds(10));
  ReportAnchorElementPointerDown(
      predictor_service.get(), anchor_id_0,
      /*navigation_start_to_pointer_down=*/base::Milliseconds(30));
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id_0,
                                /*hover_dwell_time=*/base::Milliseconds(70));
  predictor_service_host->RecordPreloadOnHoverMetrics();
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
  ReportAnchorElementPointerOver(
      predictor_service.get(), anchor_id_1,
      /*navigation_start_to_pointer_over=*/base::Milliseconds(30));
  ReportAnchorElementPointerDown(
      predictor_service.get(), anchor_id_1,
      /*navigation_start_to_pointer_down=*/base::Milliseconds(60));
  ReportAnchorElementClick(
      predictor_service.get(), anchor_id_1, target_url,
      /*navigation_start_to_click=*/base::Milliseconds(90));
  predictor_service_host->RecordPreloadOnHoverMetrics();
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
  ReportAnchorElementPointerDown(
      predictor_service.get(), anchor_id_0,
      /*navigation_start_to_pointer_down=*/base::Milliseconds(10));
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id_0,
                                /*hover_dwell_time=*/base::Milliseconds(20));
}

TEST_F(NavigationPredictorUserInteractionsTest,
       UserInteractionMetricsIsClearedAfterNavigation) {
  // Navigate to the fist page and add two anchor elements and interact with
  // them.
  NavigateAndCommit(GURL("https://www.example.com/page1.html"));
  base::RunLoop().RunUntilIdle();
  {
    mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
    auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
        main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

    ukm::TestAutoSetUkmRecorder ukm_recorder;

    auto anchor_id_0 = ReportNewAnchorElement(predictor_service.get());
    auto anchor_id_1 = ReportNewAnchorElement(predictor_service.get());

    // Both anchors enter the viewport.
    const int navigation_start_to_entered_viewport = 30;
    ReportAnchorElementEnteredViewport(
        predictor_service.get(), anchor_id_0,
        base::Milliseconds(navigation_start_to_entered_viewport));
    ReportAnchorElementEnteredViewport(
        predictor_service.get(), anchor_id_1,
        base::Milliseconds(navigation_start_to_entered_viewport));

    // Mouse hover over anchor element 0 and moves away.
    const int navigation_start_to_pointer_over_0 = 140;
    const int hover_dwell_time_0 = 60;
    ReportAnchorElementPointerOver(
        predictor_service.get(), anchor_id_0,
        base::Milliseconds(navigation_start_to_pointer_over_0));
    ReportAnchorElementPointerOut(predictor_service.get(), anchor_id_0,
                                  base::Milliseconds(hover_dwell_time_0));

    predictor_service_host->RecordUserInteractionMetrics();
    base::RunLoop().RunUntilIdle();

    // Now check the UKM records.
    using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(2u, entries.size());
  }

  // Navigate to the next page, and this time we only have 1 anchor element.
  NavigateAndCommit(GURL("https://www.example.com/page2.html"));
  base::RunLoop().RunUntilIdle();
  {
    mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
    auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
        main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

    ukm::TestAutoSetUkmRecorder ukm_recorder;
    auto anchor_id_0 = ReportNewAnchorElement(predictor_service.get(), 0);

    // The anchor enter the viewport.
    const int navigation_start_to_entered_viewport = 90;
    ReportAnchorElementEnteredViewport(
        predictor_service.get(), anchor_id_0,
        base::Milliseconds(navigation_start_to_entered_viewport));

    // Mouse hover over anchor element 0 and moves away.
    const int navigation_start_to_pointer_over_0 = 200;
    const int hover_dwell_time_0 = 20;  // it is less than 60ms
    ReportAnchorElementPointerOver(
        predictor_service.get(), anchor_id_0,
        base::Milliseconds(navigation_start_to_pointer_over_0));
    ReportAnchorElementPointerOut(predictor_service.get(), anchor_id_0,
                                  base::Milliseconds(hover_dwell_time_0));

    predictor_service_host->RecordUserInteractionMetrics();
    base::RunLoop().RunUntilIdle();

    using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
    auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto get_metric = [&](auto name) {
      return *ukm_recorder.GetEntryMetric(entries[0], name);
    };

    EXPECT_EQ(static_cast<uint32_t>(anchor_id_0),
              get_metric(UkmEntry::kAnchorIndexName));
    EXPECT_EQ(1, get_metric(UkmEntry::kIsInViewportName));
    EXPECT_EQ(1, get_metric(UkmEntry::kEnteredViewportCountName));
    EXPECT_EQ(0, get_metric(UkmEntry::kIsPointerHoveringOverName));
    EXPECT_EQ(ukm::GetExponentialBucketMin(hover_dwell_time_0, 1.3),
              get_metric(UkmEntry::kMaxHoverDwellTimeMsName));
    EXPECT_EQ(1, get_metric(UkmEntry::kPointerHoveringOverCountName));
  }
}

TEST_F(NavigationPredictorUserInteractionsTest,
       UserInteractionMetricsIgnoresNotReportedAnchorIds) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  MockNavigationPredictorForTesting::AnchorId anchor_id(0);

  // Anchor enters the viewport.
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id,
                                     base::Milliseconds(30));

  // Mouse hovers over the anchor element, pressed, and moves away.
  ReportAnchorElementPointerOver(predictor_service.get(), anchor_id,
                                 base::Milliseconds(140));
  ReportAnchorElementPointerDown(predictor_service.get(), anchor_id,
                                 base::Milliseconds(200));
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id,
                                base::Milliseconds(60));

  // Anchor leaves the viewport.
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id,
                                  base::Microseconds(300));

  predictor_service_host->RecordUserInteractionMetrics();
  base::RunLoop().RunUntilIdle();

  // Now check the UKM records.
  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(0u, entries.size());
}

// TODO(crbug.com/40266872): Flaky on Android.
TEST_F(NavigationPredictorUserInteractionsTest,
       DISABLED_UserInteractionMetricsIgnoresUpdatesForInvalidUkmSourceId) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  NavigationPredictorMetricsDocumentData* navigation_predictor_metrics_data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh());

  ukm::SourceId ukm_source_id = main_rfh()->GetPageUkmSourceId();
  navigation_predictor_metrics_data->SetUkmSourceId(ukm_source_id + 1);
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto anchor_id = ReportNewAnchorElement(predictor_service.get());

  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id,
                                     base::Milliseconds(50));
  ReportAnchorElementPointerOver(predictor_service.get(), anchor_id,
                                 base::Milliseconds(140));
  ReportAnchorElementPointerDown(predictor_service.get(), anchor_id,
                                 base::Milliseconds(200));
  ReportAnchorElementPointerOut(predictor_service.get(), anchor_id,
                                base::Milliseconds(60));
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id,
                                  base::Microseconds(300));

  EXPECT_DEATH(predictor_service_host->RecordUserInteractionMetrics(), "");
  base::RunLoop().RunUntilIdle();

  // There should be no new records.
  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(0u, entries.size());
}

TEST_F(NavigationPredictorUserInteractionsTest,
       ClickOnNotSampledAnchorElement) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  NavigationPredictorMetricsDocumentData* navigation_predictor_metrics_data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh());

  auto anchor_id = ReportNewAnchorElement(predictor_service.get());
  // Here, we simulate a not-sampled anchor by calling `ReportNewAnchorElement`
  // without dispatching `ReportAnchorElement.*Viewport` or
  // `ReportAnchorElementPointer.*` events.
  const auto navigation_start_to_click = base::Milliseconds(200);
  ReportAnchorElementClick(predictor_service.get(), anchor_id,
                           GURL("https://example.com/test.html"),
                           navigation_start_to_click);
  base::RunLoop().RunUntilIdle();
  auto anchor_index = predictor_service_host->GetAnchorIndex(anchor_id);
  EXPECT_EQ(0u,
            navigation_predictor_metrics_data->GetUserInteractionsData().count(
                anchor_index));
}

TEST_F(NavigationPredictorUserInteractionsTest,
       ProcessPointerEventUsingMLModel) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  predictor_service_host->SetTaskRunnerForTesting(
      task_runner(), task_runner()->GetMockTickClock());

  task_runner()->AdvanceMockTickClock(base::Milliseconds(150));
  auto anchor_id = ReportNewAnchorElementWithDetails(
      predictor_service.get(),
      /*ratio_area=*/0.1,
      /*ratio_distance_top_to_visible_top=*/0.0,
      /*ratio_distance_root_top=*/0.0,
      /*is_in_iframe=*/false,
      /*contains_image=*/true,
      /*is_same_host=*/true,
      /*is_url_incremented_by_one=*/true,
      /*has_text_sibling=*/false,
      /*font_size_px=*/15,
      /*font_weight=*/700);

  // Make sure the ML model is periodically called while the mouse pointer is
  // hovering over the link.
  for (int i = 0; i < 5; i++) {
    base::RunLoop run_loop;
    predictor_service_host->SetOnPreloadingHeuristicsModelDoneCallback(
        base::BindLambdaForTesting(
            [&](PreloadingModelKeyedService::Result result) {
              EXPECT_FALSE(result.has_value());
              run_loop.Quit();
            }));
    predictor_service_host->SetModelScoreCallbackForTesting(
        base::BindLambdaForTesting(
            [&](const PreloadingModelKeyedService::Inputs& inputs) {
              EXPECT_EQ(10, inputs.percent_clickable_area);
              EXPECT_EQ(2, inputs.font_size);
              EXPECT_TRUE(inputs.is_bold);
              EXPECT_FALSE(inputs.has_text_sibling);
              EXPECT_EQ(base::Milliseconds(150),
                        inputs.navigation_start_to_link_logged);
              EXPECT_EQ(base::Milliseconds(i * 100), inputs.hover_dwell_time);
            }));
    if (i == 0) {
      ProcessPointerEventUsingMLModel(
          /*predictor_service=*/predictor_service.get(),
          /*anchor_id=*/anchor_id,
          /*is_mouse=*/true,
          /*user_interaction_event_type=*/
          blink::mojom::AnchorElementUserInteractionEventForMLModelType::
              kPointerOver);
    }
    task_runner()->RunUntilIdle();
    run_loop.Run();
    task_runner()->AdvanceMockTickClock(base::Milliseconds(100));
  }

  // Make sure the model is not called after the mouse pointer out event.
  bool did_ml_score_called = false;
  predictor_service_host->SetModelScoreCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const PreloadingModelKeyedService::Inputs& inputs) {
            did_ml_score_called = true;
          }));

  ProcessPointerEventUsingMLModel(
      /*predictor_service=*/predictor_service.get(),
      /*anchor_id=*/anchor_id,
      /*is_mouse=*/true,
      /*user_interaction_event_type=*/
      blink::mojom::AnchorElementUserInteractionEventForMLModelType::
          kPointerOut);
  task_runner()->AdvanceMockTickClock(base::Milliseconds(200));
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(did_ml_score_called);

  // Navigate to trigger metrics recording.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://google.com/"), main_rfh());

  // Verify the recording of model training metrics.
  using UkmEntry =
      ukm::builders::Preloading_NavigationPredictorModelTrainingData;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(5u, entries.size());
  auto get_metric = [&](int entry_num, const auto& name) {
    return *ukm_recorder.GetEntryMetric(entries[entry_num], name);
  };
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(1, get_metric(i, UkmEntry::kIsAccurateName));
    EXPECT_EQ(0, get_metric(i, UkmEntry::kSamplingAmountName));
    EXPECT_EQ(1, get_metric(i, UkmEntry::kIsBoldName));
    EXPECT_EQ(10, get_metric(i, UkmEntry::kPercentClickableAreaName));
    constexpr double kBucketSpacing = 1.3;
    EXPECT_EQ(ukm::GetExponentialBucketMin(i * 100, kBucketSpacing),
              get_metric(i, UkmEntry::kHoverDwellTimeMsName));
  }
}

TEST_F(NavigationPredictorUserInteractionsTest, MLModelMaxHoverTime) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  predictor_service_host->SetTaskRunnerForTesting(
      task_runner(), task_runner()->GetMockTickClock());

  task_runner()->AdvanceMockTickClock(base::Milliseconds(150));
  auto anchor_id = ReportNewAnchorElementWithDetails(
      predictor_service.get(),
      /*ratio_area=*/0.1,
      /*ratio_distance_top_to_visible_top=*/0.0,
      /*ratio_distance_root_top=*/0.0,
      /*is_in_iframe=*/false,
      /*contains_image=*/true,
      /*is_same_host=*/true,
      /*is_url_incremented_by_one=*/true,
      /*has_text_sibling=*/false,
      /*font_size_px=*/15,
      /*font_weight=*/700);

  {
    base::RunLoop run_loop;
    predictor_service_host->SetModelScoreCallbackForTesting(
        base::IgnoreArgs<const PreloadingModelKeyedService::Inputs&>(
            run_loop.QuitClosure()));
    ProcessPointerEventUsingMLModel(
        /*predictor_service=*/predictor_service.get(),
        /*anchor_id=*/anchor_id,
        /*is_mouse=*/true,
        /*user_interaction_event_type=*/
        blink::mojom::AnchorElementUserInteractionEventForMLModelType::
            kPointerOver);
    task_runner()->RunUntilIdle();
    run_loop.Run();
  }

  // Stay within the hover time limit.
  task_runner()->AdvanceMockTickClock(base::Seconds(1));
  {
    base::RunLoop run_loop;
    predictor_service_host->SetModelScoreCallbackForTesting(
        base::IgnoreArgs<const PreloadingModelKeyedService::Inputs&>(
            run_loop.QuitClosure()));
    task_runner()->RunUntilIdle();
    run_loop.Run();
  }

  // Exceed the hover time limit.
  task_runner()->AdvanceMockTickClock(base::Days(1));
  {
    // The previously scheduled task will still run, but no further tasks will
    // be scheduled.
    base::RunLoop run_loop;
    predictor_service_host->SetModelScoreCallbackForTesting(
        base::IgnoreArgs<const PreloadingModelKeyedService::Inputs&>(
            run_loop.QuitClosure()));
    task_runner()->RunUntilIdle();
    run_loop.Run();
  }

  bool did_run_model = false;
  predictor_service_host->SetModelScoreCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const PreloadingModelKeyedService::Inputs& inputs) {
            did_run_model = true;
          }));
  task_runner()->AdvanceMockTickClock(base::Milliseconds(200));
  task_runner()->RunUntilIdle();
  ProcessPointerEventUsingMLModel(
      /*predictor_service=*/predictor_service.get(),
      /*anchor_id=*/anchor_id,
      /*is_mouse=*/true,
      /*user_interaction_event_type=*/
      blink::mojom::AnchorElementUserInteractionEventForMLModelType::
          kPointerOut);
  task_runner()->AdvanceMockTickClock(base::Milliseconds(200));
  task_runner()->RunUntilIdle();

  EXPECT_FALSE(did_run_model);
}

TEST_F(NavigationPredictorTest, RemoveAnchorElement) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  EXPECT_EQ(0u, predictor_service_host->NumAnchorElementData());

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics1;
  blink::mojom::AnchorElementMetricsPtr anchor1 = CreateMetricsPtr();
  metrics1.push_back(anchor1->Clone());
  uint32_t anchor1_id = metrics1[0]->anchor_id;
  predictor_service->ReportNewAnchorElements(std::move(metrics1),
                                             /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, predictor_service_host->NumAnchorElementData());
  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          main_rfh())
          ->GetAnchorsData();
  EXPECT_EQ(1u, data.number_of_anchors_);

  // Report the addition of another anchor and report that the first anchor was
  // removed.
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics2;
  metrics2.push_back(CreateMetricsPtr());
  predictor_service->ReportNewAnchorElements(std::move(metrics2),
                                             /*removed_elements=*/{anchor1_id});
  base::RunLoop().RunUntilIdle();
  // We drop the information about the removed element in order to save memory.
  EXPECT_EQ(1u, predictor_service_host->NumAnchorElementData());
  EXPECT_EQ(2u, data.number_of_anchors_);

  // Suppose the first anchor was reinserted.
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics3;
  metrics3.push_back(anchor1->Clone());
  predictor_service->ReportNewAnchorElements(std::move(metrics3),
                                             /*removed_elements=*/{});
  base::RunLoop().RunUntilIdle();
  // We start storing the information about this element again.
  EXPECT_EQ(2u, predictor_service_host->NumAnchorElementData());
  // We've seen the same element previously, so we don't consider this an
  // additional anchor.
  EXPECT_EQ(2u, data.number_of_anchors_);
}

TEST_F(NavigationPredictorUserInteractionsTest,
       ReportAnchorElementsPositionUpdate_BadMessage) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kNavigationPredictorNewViewportFeatures);
  mojo::test::BadMessageObserver bad_message_observer;
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());

  auto anchor_id_1 = ReportNewAnchorElement(predictor_service.get());
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id_1,
                                     base::Milliseconds(200));

  ReportAnchorElementPositionUpdate(predictor_service.get(), anchor_id_1,
                                    0.456f, 0.123f);
  EXPECT_EQ(
      "ReportAnchorElementsPositionUpdate should only be called with "
      "kNavigationPredictorNewViewportFeatures enabled.",
      bad_message_observer.WaitForBadMessage());
}

class NavigationPredictorNewViewportFeaturesTest
    : public NavigationPredictorUserInteractionsTest {
 public:
  void SetUp() override {
    NavigationPredictorUserInteractionsTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNavigationPredictorNewViewportFeatures);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NavigationPredictorNewViewportFeaturesTest, RecordPositionMetrics) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto anchor_id_1 = ReportNewAnchorElement(predictor_service.get());
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id_1,
                                     base::Milliseconds(200));
  auto anchor_id_2 = ReportNewAnchorElement(predictor_service.get());
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id_2,
                                     base::Milliseconds(200));
  EXPECT_EQ(2u, predictor_service_host->user_interactions().size());
  const auto& user_interactions_1 =
      predictor_service_host->user_interaction(anchor_id_1);
  EXPECT_TRUE(user_interactions_1.is_in_viewport);
  EXPECT_FALSE(
      user_interactions_1.percent_distance_from_pointer_down.has_value());
  const auto& user_interactions_2 =
      predictor_service_host->user_interaction(anchor_id_2);
  EXPECT_TRUE(user_interactions_2.is_in_viewport);
  EXPECT_FALSE(
      user_interactions_2.percent_distance_from_pointer_down.has_value());

  ReportAnchorElementPositionUpdate(predictor_service.get(), anchor_id_1,
                                    0.456f, 0.123f);
  EXPECT_EQ(45, user_interactions_1.percent_vertical_position.value());
  EXPECT_EQ(12, user_interactions_1.percent_distance_from_pointer_down.value());
  ReportAnchorElementPositionUpdate(predictor_service.get(), anchor_id_2,
                                    -0.123f, -0.256f);
  EXPECT_EQ(-12, user_interactions_2.percent_vertical_position.value());
  EXPECT_EQ(-25,
            user_interactions_2.percent_distance_from_pointer_down.value());

  predictor_service_host->RecordUserInteractionMetrics();
  base::RunLoop().RunUntilIdle();

  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(2u, entries.size());
  std::vector<std::tuple<int, int, int>> recorded_metrics(entries.size());
  base::ranges::transform(
      entries, recorded_metrics.begin(), [&ukm_recorder](const auto& entry) {
        auto anchor_id = static_cast<int>(
            *ukm_recorder.GetEntryMetric(entry, UkmEntry::kAnchorIndexName));
        auto position = static_cast<int>(*ukm_recorder.GetEntryMetric(
            entry, UkmEntry::kVerticalPositionInViewportName));
        auto distance = static_cast<int>(*ukm_recorder.GetEntryMetric(
            entry, UkmEntry::kDistanceFromLastPointerDownName));
        return std::make_tuple(anchor_id, position, distance);
      });
  EXPECT_THAT(
      recorded_metrics,
      ::testing::UnorderedElementsAre(
          ::testing::FieldsAre(
              predictor_service_host->GetAnchorIndex(anchor_id_1), 40, 10),
          ::testing::FieldsAre(
              predictor_service_host->GetAnchorIndex(anchor_id_2), 0, -30)));
}

TEST_F(NavigationPredictorNewViewportFeaturesTest,
       RecordPositionMetrics_NotInViewport) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto anchor_id = ReportNewAnchorElement(predictor_service.get());
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id,
                                     base::Milliseconds(200));
  ReportAnchorElementPositionUpdate(predictor_service.get(), anchor_id, 0.123f,
                                    0.256f);
  ReportAnchorElementLeftViewport(predictor_service.get(), anchor_id,
                                  base::Milliseconds(500));

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  const auto& user_interactions =
      predictor_service_host->user_interaction(anchor_id);
  EXPECT_FALSE(user_interactions.is_in_viewport);
  // The value should be reset when the anchor leaves the viewport.
  EXPECT_FALSE(
      user_interactions.percent_distance_from_pointer_down.has_value());

  predictor_service_host->RecordUserInteractionMetrics();
  base::RunLoop().RunUntilIdle();

  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(
      0, *ukm_recorder.GetEntryMetric(entries[0], UkmEntry::kIsInViewportName));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entries[0], UkmEntry::kVerticalPositionInViewportName));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entries[0], UkmEntry::kDistanceFromLastPointerDownName));
}

TEST_F(NavigationPredictorNewViewportFeaturesTest,
       RecordPositionMetrics_NoDistanceFromPointerDown) {
  mojo::Remote<blink::mojom::AnchorElementMetricsHost> predictor_service;
  auto* predictor_service_host = MockNavigationPredictorForTesting::Create(
      main_rfh(), predictor_service.BindNewPipeAndPassReceiver());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto anchor_id = ReportNewAnchorElement(predictor_service.get());
  ReportAnchorElementEnteredViewport(predictor_service.get(), anchor_id,
                                     base::Milliseconds(200));
  ReportAnchorElementPositionUpdate(predictor_service.get(), anchor_id, 0.123f,
                                    std::nullopt);

  EXPECT_EQ(1u, predictor_service_host->user_interactions().size());
  const auto& user_interactions =
      predictor_service_host->user_interaction(anchor_id);
  EXPECT_TRUE(user_interactions.is_in_viewport);
  EXPECT_EQ(12, user_interactions.percent_vertical_position);
  EXPECT_FALSE(
      user_interactions.percent_distance_from_pointer_down.has_value());

  predictor_service_host->RecordUserInteractionMetrics();
  base::RunLoop().RunUntilIdle();

  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(10, *ukm_recorder.GetEntryMetric(
                    entries[0], UkmEntry::kVerticalPositionInViewportName));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entries[0], UkmEntry::kDistanceFromLastPointerDownName));
}
