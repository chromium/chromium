// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/translate_page_load_metrics_observer.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"

std::unique_ptr<TranslatePageLoadMetricsObserver>
TranslatePageLoadMetricsObserver::CreateIfNeeded(
    content::WebContents* web_contents) {
  // Gets the |TranslateManager| associated with the same |WebContents| as this
  // |TranslatePageLoadMetricsObserver|.
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents);
  if (translate_manager == nullptr)
    return nullptr;

  // Creates a new |TranslateMetricsLoggerImpl| and associates with the above
  // |TranslateManager|.
  std::unique_ptr<translate::TranslateMetricsLogger> translate_metrics_logger =
      std::make_unique<translate::TranslateMetricsLoggerImpl>(
          translate_manager->GetWeakPtr());
  return std::make_unique<TranslatePageLoadMetricsObserver>(
      std::move(translate_metrics_logger));
}

TranslatePageLoadMetricsObserver::TranslatePageLoadMetricsObserver(
    std::unique_ptr<translate::TranslateMetricsLogger> translate_metrics_logger)
    : translate_metrics_logger_(std::move(translate_metrics_logger)) {}

TranslatePageLoadMetricsObserver::~TranslatePageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TranslatePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  translate_metrics_logger_->OnPageLoadStart(started_in_foreground);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TranslatePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  translate_metrics_logger_->SetUkmSourceId(source_id);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TranslatePageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  translate_metrics_logger_->OnForegroundChange(false);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TranslatePageLoadMetricsObserver::OnShown() {
  translate_metrics_logger_->OnForegroundChange(true);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TranslatePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  translate_metrics_logger_->RecordMetrics(false);
  return CONTINUE_OBSERVING;
}

void TranslatePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  translate_metrics_logger_->RecordMetrics(true);
}
