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
  DetectNextJS();
}

void JavascriptFrameworksUkmObserver::DetectNextJS() {
  if (nextjs_detected_) {
    return;
  }
  nextjs_detected_ =
      (GetDelegate().GetMainFrameMetadata().behavior_flags &
       blink::LoadingBehaviorFlag::kLoadingBehaviorNextJSFrameworkUsed) != 0;
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
  builder.SetNextJSPageLoad(nextjs_detected_);
  builder.Record(ukm::UkmRecorder::Get());
}
