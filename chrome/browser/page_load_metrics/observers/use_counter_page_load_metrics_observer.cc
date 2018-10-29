// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/use_counter/ukm_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"

using WebFeature = blink::mojom::WebFeature;
using Features = page_load_metrics::mojom::PageLoadFeatures;

UseCounterPageLoadMetricsObserver::UseCounterPageLoadMetricsObserver() {}
UseCounterPageLoadMetricsObserver::~UseCounterPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // Verify that no feature usage is observed before commit
  DCHECK(features_recorded_.count() <= 0);
  ukm::builders::Blink_UseCounter(source_id)
      .SetFeature(static_cast<int64_t>(WebFeature::kPageVisits))
      .Record(ukm::UkmRecorder::Get());
  UMA_HISTOGRAM_ENUMERATION(internal::kFeaturesHistogramName,
                            WebFeature::kPageVisits,
                            WebFeature::kNumberOfFeatures);
  UMA_HISTOGRAM_ENUMERATION(internal::kCssPropertiesHistogramName,
                            blink::mojom::kTotalPagesMeasuredCSSSampleId,
                            blink::mojom::kMaximumCSSSampleId);
  UMA_HISTOGRAM_ENUMERATION(internal::kAnimatedCssPropertiesHistogramName,
                            blink::mojom::kTotalPagesMeasuredCSSSampleId,
                            blink::mojom::kMaximumCSSSampleId);
  features_recorded_.set(static_cast<size_t>(WebFeature::kPageVisits));
  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::OnFeaturesUsageObserved(
    const Features& features,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  for (WebFeature feature : features.features) {
    // Verify that kPageVisits is observed at most once per observer.
    if (feature == WebFeature::kPageVisits) {
      mojo::ReportBadMessage(
          "kPageVisits should not be passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    // The usage of each feature should be only measured once. With OOPIF,
    // multiple child frames may send the same feature to the browser, skip if
    // feature has already been measured.
    if (features_recorded_.test(static_cast<size_t>(feature)))
      continue;
    UMA_HISTOGRAM_ENUMERATION(internal::kFeaturesHistogramName, feature,
                              WebFeature::kNumberOfFeatures);
    features_recorded_.set(static_cast<size_t>(feature));
    // TODO(kochi): https://crbug.com/806671 https://843080
    // as ElementCreateShadowRoot is ~8% and
    // DocumentRegisterElement is ~5% as of May, 2018, to meet UKM's data
    // volume expectation, reduce the data size by sampling. Revisit and
    // remove this code once Shadow DOM V0 and Custom Elements V0 are removed.
    const int kSamplingFactor = 10;
    if ((feature == WebFeature::kElementCreateShadowRoot ||
         feature == WebFeature::kDocumentRegisterElement) &&
        base::RandGenerator(kSamplingFactor) != 0)
      continue;
    if (IsAllowedUkmFeature(feature)) {
      ukm::builders::Blink_UseCounter(extra_info.source_id)
          .SetFeature(static_cast<int64_t>(feature))
          .Record(ukm::UkmRecorder::Get());
    }
  }

  for (int css_property : features.css_properties) {
    // Verify that page visit is observed at most once per observer.
    if (css_property == blink::mojom::kTotalPagesMeasuredCSSSampleId) {
      mojo::ReportBadMessage(
          "kTotalPagesMeasuredCSSSampleId should not be passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    if (css_property > blink::mojom::kMaximumCSSSampleId) {
      mojo::ReportBadMessage(
          "Invalid CSS property passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    // Same as above, the usage of each CSS property should be only measured
    // once.
    if (css_properties_recorded_.test(css_property))
      continue;
    // There are about 600 enums, so the memory required for a vector histogram
    // is about 600 * 8 byes = 5KB
    // 50% of the time there are about 100 CSS properties recorded per page
    // load. Storage in sparce histogram entries are 48 bytes instead of 8
    // bytes so the memory required for a sparse histogram is about
    // 100 * 48 bytes = 5KB. On top there will be std::map overhead and the
    // acquire/release of a base::Lock to protect the map during each update.
    // Overal it is still better to use a vector histogram here since it is
    // faster to access and merge and uses about same amount of memory.
    UMA_HISTOGRAM_ENUMERATION(internal::kCssPropertiesHistogramName,
                              css_property, blink::mojom::kMaximumCSSSampleId);
    css_properties_recorded_.set(css_property);
  }

  for (int animated_css_property : features.animated_css_properties) {
    // Verify that page visit is observed at most once per observer.
    if (animated_css_property == blink::mojom::kTotalPagesMeasuredCSSSampleId) {
      mojo::ReportBadMessage(
          "kTotalPagesMeasuredCSSSampleId should not be passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    if (animated_css_property > blink::mojom::kMaximumCSSSampleId) {
      mojo::ReportBadMessage(
          "Invalid animated CSS property passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    // Same as above, the usage of each animated CSS property should be only
    // measured once.
    if (animated_css_properties_recorded_.test(animated_css_property))
      continue;
    // See comments above (in the css property section) for reasoning of using
    // a vector histogram here instead of a sparse histogram.
    UMA_HISTOGRAM_ENUMERATION(internal::kAnimatedCssPropertiesHistogramName,
                              animated_css_property,
                              blink::mojom::kMaximumCSSSampleId);
    animated_css_properties_recorded_.set(animated_css_property);
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
                     CONTINUE_OBSERVING ||
                 mime_type == "image/svg+xml"
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}
