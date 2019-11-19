// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

namespace previews {

namespace {

using page_load_metrics::mojom::ResourceDataUpdate;
using page_load_metrics::mojom::ResourceDataUpdatePtr;

const char kDefaultTestUrl[] = "https://www.google.com";

class TestPreviewsPageLoadMetricsObserver
    : public PreviewsPageLoadMetricsObserver {
 public:
  TestPreviewsPageLoadMetricsObserver(
      base::OnceCallback<void(const GURL&, int64_t)> bytes_callback)
      : bytes_callback_(std::move(bytes_callback)) {}
  ~TestPreviewsPageLoadMetricsObserver() override {}

  void WriteToSavings(const GURL& url, int64_t bytes_savings) override {
    std::move(bytes_callback_).Run(url, bytes_savings);
  }

 private:
  base::OnceCallback<void(const GURL&, int64_t)> bytes_callback_;
};

class PreviewsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsPageLoadMetricsObserverTest() {}
  ~PreviewsPageLoadMetricsObserverTest() override {}

  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_timing->parse_start = base::TimeDelta::FromSeconds(3);
    timing_.paint_timing->first_contentful_paint =
        base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_paint = base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromSeconds(8);
    timing_.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.document_timing->load_event_start = base::TimeDelta::FromSeconds(7);
    timing_.parse_timing->parse_stop = base::TimeDelta::FromSeconds(4);
    timing_.parse_timing->parse_blocked_on_script_load_duration =
        base::TimeDelta::FromSeconds(1);
    PopulateRequiredTimingFields(&timing_);
  }

  content::GlobalRequestID NavigateAndCommitWithPreviewsState(
      content::PreviewsState previews_state) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL(kDefaultTestUrl), main_rfh());
    navigation_simulator->Start();
    PreviewsUITabHelper::FromWebContents(web_contents())
        ->CreatePreviewsUserDataForNavigationHandle(
            navigation_simulator->GetNavigationHandle(), 1u)
        ->set_committed_previews_state(previews_state);
    navigation_simulator->Commit();
    return navigation_simulator->GetGlobalRequestID();
  }

  void ValidateTimingHistograms(std::string preview_type_name,
                                bool preview_was_active) {
    ValidateTimingHistogram("PageLoad.Clients." + preview_type_name +
                                ".DocumentTiming."
                                "NavigationToLoadEventFired",
                            timing_.document_timing->load_event_start,
                            preview_was_active);
    ValidateTimingHistogram("PageLoad.Clients." + preview_type_name +
                                ".PaintTiming."
                                "NavigationToFirstContentfulPaint",
                            timing_.paint_timing->first_contentful_paint,
                            preview_was_active);
    ValidateTimingHistogram("PageLoad.Clients." + preview_type_name +
                                ".Experimental.PaintTiming."
                                "NavigationToFirstMeaningfulPaint",
                            timing_.paint_timing->first_meaningful_paint,
                            preview_was_active);
    ValidateTimingHistogram(
        "PageLoad.Clients." + preview_type_name +
            ".ParseTiming.ParseBlockedOnScriptLoad",
        timing_.parse_timing->parse_blocked_on_script_load_duration,
        preview_was_active);
    ValidateTimingHistogram(
        "PageLoad.Clients." + preview_type_name + ".ParseTiming.ParseDuration",
        timing_.parse_timing->parse_stop.value() -
            timing_.parse_timing->parse_start.value(),
        preview_was_active);
  }

  void ValidateTimingHistogram(const std::string& histogram,
                               const base::Optional<base::TimeDelta>& event,
                               bool preview_was_active) {
    tester()->histogram_tester().ExpectTotalCount(histogram,
                                                  preview_was_active ? 1 : 0);
    if (!preview_was_active) {
      tester()->histogram_tester().ExpectTotalCount(histogram, 0);
    } else {
      tester()->histogram_tester().ExpectUniqueSample(
          histogram,
          static_cast<base::HistogramBase::Sample>(
              event.value().InMilliseconds()),
          1);
    }
  }

  void ValidateDataHistograms(std::string preview_type_name,
                              int network_resources,
                              int64_t network_bytes) {
    if (network_resources > 0) {
      tester()->histogram_tester().ExpectUniqueSample(
          "PageLoad.Clients." + preview_type_name +
              ".Experimental.Bytes.NetworkIncludingHeaders",
          static_cast<int>(network_bytes / 1024), 1);
    } else {
      tester()->histogram_tester().ExpectTotalCount(
          "PageLoad.Clients." + preview_type_name +
              ".Experimental.Bytes.NetworkIncludingHeaders",
          0);
    }
  }

  void WriteToSavings(const GURL& url, int64_t bytes_savings) {
    savings_url_ = url;
    bytes_savings_ = bytes_savings;
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<TestPreviewsPageLoadMetricsObserver>(
        base::BindOnce(&PreviewsPageLoadMetricsObserverTest::WriteToSavings,
                       base::Unretained(this))));
  }

  page_load_metrics::mojom::PageLoadTiming timing_;

  GURL savings_url_;
  int64_t bytes_savings_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreviewsPageLoadMetricsObserverTest);
};

TEST_F(PreviewsPageLoadMetricsObserverTest, NoActivePreview) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kNoScriptPreviews);
  ResetTest();

  NavigateAndCommitWithPreviewsState(content::PREVIEWS_OFF);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);

  tester()->SimulateTimingUpdate(timing_);
  tester()->NavigateToUntrackedUrl();

  ValidateTimingHistograms("NoScriptPreview", false /* preview_was_active */);
  ValidateTimingHistograms("ResourceLoadingHintsPreview",
                           false /* preview_was_active */);
  ValidateDataHistograms("NoScriptPreview", 0 /* network_resources */,
                         0 /* network_bytes */);
  ValidateDataHistograms("ResourceLoadingHintsPreview",
                         0 /* network_resources */, 0 /* network_bytes */);
}

TEST_F(PreviewsPageLoadMetricsObserverTest, NoScriptPreviewActive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kNoScriptPreviews);
  ResetTest();

  NavigateAndCommitWithPreviewsState(content::NOSCRIPT_ON);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);

  tester()->SimulateTimingUpdate(timing_);
  tester()->NavigateToUntrackedUrl();

  ValidateTimingHistograms("NoScriptPreview", true /* preview_was_active */);
  ValidateDataHistograms("NoScriptPreview", 1 /* network_resources */,
                         20 * 1024 /* network_bytes */);
}

TEST_F(PreviewsPageLoadMetricsObserverTest, ResourceLoadingHintsPreviewActive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kResourceLoadingHints);
  ResetTest();

  NavigateAndCommitWithPreviewsState(content::RESOURCE_LOADING_HINTS_ON);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);

  tester()->SimulateTimingUpdate(timing_);
  tester()->NavigateToUntrackedUrl();

  ValidateTimingHistograms("ResourceLoadingHintsPreview",
                           true /* preview_was_active */);
  ValidateDataHistograms("ResourceLoadingHintsPreview",
                         1 /* network_resources */,
                         20 * 1024 /* network_bytes */);
}

TEST_F(PreviewsPageLoadMetricsObserverTest, NoScriptDataSavings) {
  int inflation = 50;
  int constant_savings = 120;
  base::test::ScopedFeatureList scoped_feature_list;

  std::map<std::string, std::string> parameters = {
      {"NoScriptInflationPercent", base::NumberToString(inflation)},
      {"NoScriptInflationBytes", base::NumberToString(constant_savings)}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      previews::features::kNoScriptPreviews, parameters);

  ResetTest();

  int64_t data_use = 0;
  NavigateAndCommitWithPreviewsState(content::NOSCRIPT_ON);
  std::vector<ResourceDataUpdatePtr> resources;
  auto resource_data_update = ResourceDataUpdate::New();
  resource_data_update->delta_bytes = 5 * 1024;
  resources.push_back(std::move(resource_data_update));
  tester()->SimulateResourceDataUseUpdate(resources);
  data_use += (5 * 1024);

  resources.clear();

  resource_data_update = ResourceDataUpdate::New();
  resource_data_update->delta_bytes = 20 * 1024;
  resources.push_back(std::move(resource_data_update));
  tester()->SimulateResourceDataUseUpdate(resources);
  data_use += (20 * 1024);

  tester()->SimulateTimingUpdate(timing_);

  int64_t expected_savings = (data_use * inflation) / 100 + constant_savings;

  EXPECT_EQ(GURL(kDefaultTestUrl), savings_url_);
  EXPECT_EQ(expected_savings, bytes_savings_);
}

TEST_F(PreviewsPageLoadMetricsObserverTest, ResourceLoadingHintsDataSavings) {
  int inflation = 30;
  int constant_savings = 300;
  base::test::ScopedFeatureList scoped_feature_list;

  std::map<std::string, std::string> parameters = {
      {"ResourceLoadingHintsInflationPercent", base::NumberToString(inflation)},
      {"ResourceLoadingHintsInflationBytes",
       base::NumberToString(constant_savings)}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      previews::features::kResourceLoadingHints, parameters);

  ResetTest();

  int64_t data_use = 0;
  NavigateAndCommitWithPreviewsState(content::RESOURCE_LOADING_HINTS_ON);
  std::vector<ResourceDataUpdatePtr> resources;
  auto resource_data_update = ResourceDataUpdate::New();
  resource_data_update->delta_bytes = 5 * 1024;
  resources.push_back(std::move(resource_data_update));
  tester()->SimulateResourceDataUseUpdate(resources);
  data_use += (5 * 1024);

  resources.clear();

  resource_data_update = ResourceDataUpdate::New();
  resource_data_update->delta_bytes = 20 * 1024;
  resources.push_back(std::move(resource_data_update));
  tester()->SimulateResourceDataUseUpdate(resources);
  data_use += (20 * 1024);

  tester()->SimulateTimingUpdate(timing_);

  int64_t expected_savings = (data_use * inflation) / 100 + constant_savings;

  EXPECT_EQ(GURL(kDefaultTestUrl), savings_url_);
  EXPECT_EQ(expected_savings, bytes_savings_);
}

}  // namespace

}  //  namespace previews
