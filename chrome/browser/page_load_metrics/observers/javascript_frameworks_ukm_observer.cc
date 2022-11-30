// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/javascript_frameworks_ukm_observer.h"

#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

JavascriptFrameworksUkmObserver::JavascriptFrameworksUkmObserver() = default;

JavascriptFrameworksUkmObserver::~JavascriptFrameworksUkmObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
JavascriptFrameworksUkmObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // OnLoadingBehaviorObserved events for detecting JavaScript frameworks are
  // only kicked for outermost frames. See DetectJavascriptFrameworksOnLoad in
  // third_party/blink/renderer/core/script/detect_javascript_frameworks.cc
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
JavascriptFrameworksUkmObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Metrics should be collected for Prerendered frames but only recorded after
  // the page has been displayed.
  is_in_prerendered_page_ = true;
  return CONTINUE_OBSERVING;
}

void JavascriptFrameworksUkmObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flag) {
  // This will add bits corresponding to detected frameworks in |behavior_flag|
  // to |frameworks_detected_|. It may also add other bits, which we don't care
  // about.
  frameworks_detected_ |= behavior_flag;
}

void JavascriptFrameworksUkmObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming&) {
  if (is_in_prerendered_page_)
    return;

  RecordJavascriptFrameworkPageLoad();
}

JavascriptFrameworksUkmObserver::ObservePolicy
JavascriptFrameworksUkmObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming&) {
  if (is_in_prerendered_page_)
    return CONTINUE_OBSERVING;

  RecordJavascriptFrameworkPageLoad();
  return STOP_OBSERVING;
}

void JavascriptFrameworksUkmObserver::RecordJavascriptFrameworkPageLoad() {
  ukm::builders::JavascriptFrameworkPageLoad builder(
      GetDelegate().GetPageUkmSourceId());
  builder
      .SetGatsbyPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorGatsbyFrameworkUsed) !=
          0)
      .SetNextJSPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorNextJSFrameworkUsed) !=
          0)
      .SetNuxtJSPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorNuxtJSFrameworkUsed) !=
          0)
      .SetSapperPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorSapperFrameworkUsed) !=
          0)
      .SetVuePressPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorVuePressFrameworkUsed) !=
          0)
      .SetAngularPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorAngularFrameworkUsed) !=
          0)
      .SetPreactPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorPreactFrameworkUsed) !=
          0)
      .SetReactPageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorReactFrameworkUsed) != 0)
      .SetSveltePageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorSvelteFrameworkUsed) !=
          0)
      .SetVuePageLoad(
          (frameworks_detected_ &
           blink::LoadingBehaviorFlag::kLoadingBehaviorVueFrameworkUsed) != 0);
  builder.Record(ukm::UkmRecorder::Get());
}

void JavascriptFrameworksUkmObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  DCHECK(is_in_prerendered_page_);
  is_in_prerendered_page_ = false;
}
