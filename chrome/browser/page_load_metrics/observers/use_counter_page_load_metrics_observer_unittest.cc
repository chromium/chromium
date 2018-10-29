// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"

#include <memory>
#include <vector>
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/web_feature.mojom.h"
#include "url/gurl.h"

namespace {

const char kTestUrl[] = "https://www.google.com";
using WebFeature = blink::mojom::WebFeature;

}  // namespace

class UseCounterPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  UseCounterPageLoadMetricsObserverTest() {}

  void HistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
    NavigateAndCommit(GURL(kTestUrl));
    SimulateFeaturesUpdate(first_features);
    // Verify that kPageVisits is observed on commit.
    histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    // Verify that page visit is recorded for CSS histograms.
    histogram_tester().ExpectBucketCount(
        internal::kCssPropertiesHistogramName,
        blink::mojom::kTotalPagesMeasuredCSSSampleId, 1);
    histogram_tester().ExpectBucketCount(
        internal::kAnimatedCssPropertiesHistogramName,
        blink::mojom::kTotalPagesMeasuredCSSSampleId, 1);

    for (auto feature : first_features.features) {
      histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }

    SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.features) {
      histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
    for (auto feature : second_features.features) {
      histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
  }

  void CssHistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
    NavigateAndCommit(GURL(kTestUrl));
    SimulateFeaturesUpdate(first_features);
    // Verify that page visit is recorded for CSS histograms.
    histogram_tester().ExpectBucketCount(
        internal::kCssPropertiesHistogramName,
        blink::mojom::kTotalPagesMeasuredCSSSampleId, 1);

    for (auto feature : first_features.css_properties) {
      histogram_tester().ExpectBucketCount(
          internal::kCssPropertiesHistogramName, feature, 1);
    }

    SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.css_properties) {
      histogram_tester().ExpectBucketCount(
          internal::kCssPropertiesHistogramName, feature, 1);
    }
    for (auto feature : second_features.css_properties) {
      histogram_tester().ExpectBucketCount(
          internal::kCssPropertiesHistogramName, feature, 1);
    }
  }

  void AnimatedCssHistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
    NavigateAndCommit(GURL(kTestUrl));
    SimulateFeaturesUpdate(first_features);
    // Verify that page visit is recorded for CSS histograms.
    histogram_tester().ExpectBucketCount(
        internal::kAnimatedCssPropertiesHistogramName,
        blink::mojom::kTotalPagesMeasuredCSSSampleId, 1);

    for (auto feature : first_features.animated_css_properties) {
      histogram_tester().ExpectBucketCount(
          internal::kAnimatedCssPropertiesHistogramName, feature, 1);
    }

    SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.animated_css_properties) {
      histogram_tester().ExpectBucketCount(
          internal::kAnimatedCssPropertiesHistogramName, feature, 1);
    }
    for (auto feature : second_features.animated_css_properties) {
      histogram_tester().ExpectBucketCount(
          internal::kAnimatedCssPropertiesHistogramName, feature, 1);
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UseCounterPageLoadMetricsObserverTest);
};

TEST_F(UseCounterPageLoadMetricsObserverTest, CountOneFeature) {
  std::vector<WebFeature> features({WebFeature::kFetch});
  page_load_metrics::mojom::PageLoadFeatures page_load_features;
  page_load_features.features = features;
  HistogramBasicTest(page_load_features);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1({WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountDuplicatedFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1(
      {WebFeature::kFetch, WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordUkmUsage) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kNavigatorVibrate});
  std::vector<WebFeature> features_1(
      {WebFeature::kTouchEventPreventedNoTouchAction});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      test_ukm_recorder().GetEntriesByName(
          ukm::builders::Blink_UseCounter::kEntryName);
  EXPECT_EQ(3ul, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestUrl));
  test_ukm_recorder().ExpectEntryMetric(
      entries[0], ukm::builders::Blink_UseCounter::kFeatureName,
      static_cast<int64_t>(WebFeature::kPageVisits));
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[1], GURL(kTestUrl));
  test_ukm_recorder().ExpectEntryMetric(
      entries[1], ukm::builders::Blink_UseCounter::kFeatureName,
      static_cast<int64_t>(WebFeature::kNavigatorVibrate));
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[2], GURL(kTestUrl));
  test_ukm_recorder().ExpectEntryMetric(
      entries[2], ukm::builders::Blink_UseCounter::kFeatureName,
      static_cast<int64_t>(WebFeature::kTouchEventPreventedNoTouchAction));
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordCSSProperties) {
  // CSSPropertyFont (5), CSSPropertyZoom (19)
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.css_properties = {5, 19};
  page_load_features_1.css_properties = {19};
  CssHistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordAnimatedCSSProperties) {
  // CSSPropertyFont (5), CSSPropertyZoom (19)
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.css_properties = {5, 19};
  page_load_features_1.css_properties = {19};
  AnimatedCssHistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordCSSPropertiesInRange) {
  page_load_metrics::mojom::PageLoadFeatures page_load_features;
  page_load_features.css_properties = {2, blink::mojom::kMaximumCSSSampleId};
  CssHistogramBasicTest(page_load_features);
}
