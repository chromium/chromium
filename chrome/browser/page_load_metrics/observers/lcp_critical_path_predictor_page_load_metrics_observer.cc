// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

PAGE_USER_DATA_KEY_IMPL(
    LcpCriticalPathPredictorPageLoadMetricsObserver::PageData);

LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::PageData(
    content::Page& page)
    : content::PageUserData<PageData>(page) {}

LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::~PageData() =
    default;

// static
std::unique_ptr<LcpCriticalPathPredictorPageLoadMetricsObserver>
LcpCriticalPathPredictorPageLoadMetricsObserver::CreateIfNeeded(
    content::WebContents* web_contents) {
  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!loading_predictor) {
    return nullptr;
  }
  return base::WrapUnique(new LcpCriticalPathPredictorPageLoadMetricsObserver(
      *loading_predictor->resource_prefetch_predictor()));
}

LcpCriticalPathPredictorPageLoadMetricsObserver::
    LcpCriticalPathPredictorPageLoadMetricsObserver(
        predictors::ResourcePrefetchPredictor& predictor)
    : predictor_(predictor) {}

LcpCriticalPathPredictorPageLoadMetricsObserver::
    ~LcpCriticalPathPredictorPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  commit_url_ = navigation_handle->GetURL();
  LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::GetOrCreateForPage(
      GetDelegate().GetWebContents()->GetPrimaryPage())
      ->SetLcpCriticalPathPredictorPageLoadMetricsObserver(
          weak_factory_.GetWeakPtr());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/1468188): Currently, LCPP doesn't support prerendered cases
  // since prerendered cases are different from the normal navigation. Revisit
  // here when we decided to support prerendered cases.
  return STOP_OBSERVING;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  FinalizeLCPPSignals();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::
    FlushMetricsOnAppEnterBackground(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This follows UmaPageLoadMetricsObserver.
  if (GetDelegate().DidCommit()) {
    FinalizeLCPPSignals();
  }
  return STOP_OBSERVING;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::FinalizeLCPPSignals() {
  if (!commit_url_ || !lcp_element_locator_) {
    return;
  }

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();

  if (largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    predictor_->LearnLcpp(commit_url_->host(), *lcp_element_locator_);
  }
}
