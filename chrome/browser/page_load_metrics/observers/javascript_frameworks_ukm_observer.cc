// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/javascript_frameworks_ukm_observer.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

JavascriptFrameworksUkmObserver::JavascriptFrameworksUkmObserver() = default;

JavascriptFrameworksUkmObserver::~JavascriptFrameworksUkmObserver() = default;

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
  RecordJavascriptFrameworkPageLoad();
}

JavascriptFrameworksUkmObserver::ObservePolicy
JavascriptFrameworksUkmObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming&) {
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
          0);
  builder.Record(ukm::UkmRecorder::Get());
}
