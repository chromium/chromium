// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/amp_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/data_saver_site_breakdown_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/live_tab_count_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/loading_predictor_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/local_network_requests_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/lofi_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/media_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/no_state_prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/offline_page_previews_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_capping_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/prerender_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"
#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/security_state_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/signed_exchange_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/tab_restore_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"
#else
#include "chrome/browser/page_load_metrics/observers/session_restore_page_load_metrics_observer.h"
#endif

namespace chrome {

namespace {

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderInterface {
 public:
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents);
  ~PageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderInterface:
  bool IsNewTabPageUrl(const GURL& url) override;
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override;
  std::unique_ptr<base::OneShotTimer> CreateTimer() override;

 private:
  bool IsPrerendering() const;

  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsEmbedder);
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() {}

void PageLoadMetricsEmbedder::RegisterObservers(
    page_load_metrics::PageLoadTracker* tracker) {
  if (!IsPrerendering()) {
    tracker->AddObserver(std::make_unique<AbortsPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<AMPPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<CorePageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<
            data_reduction_proxy::DataReductionProxyMetricsObserver>());
    tracker->AddObserver(std::make_unique<SchemePageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<data_reduction_proxy::LoFiPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<FromGWSPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<DocumentWritePageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<LiveTabCountPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<MediaPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<MultiTabLoadingPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<
            previews::OfflinePagePreviewsPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<PageCappingPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<previews::PreviewsPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<previews::PreviewsUKMObserver>());
    tracker->AddObserver(
        std::make_unique<ServiceWorkerPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<SignedExchangePageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<HttpsEngagementPageLoadMetricsObserver>(
            web_contents_->GetBrowserContext()));
    tracker->AddObserver(std::make_unique<ProtocolPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<TabRestorePageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<DataSaverSiteBreakdownMetricsObserver>());
    std::unique_ptr<AdsPageLoadMetricsObserver> ads_observer =
        AdsPageLoadMetricsObserver::CreateIfNeeded();
    if (ads_observer)
      tracker->AddObserver(std::move(ads_observer));

    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver> ukm_observer =
        UkmPageLoadMetricsObserver::CreateIfNeeded();
    if (ukm_observer)
      tracker->AddObserver(std::move(ukm_observer));

    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        no_state_prefetch_observer =
            NoStatePrefetchPageLoadMetricsObserver::CreateIfNeeded(
                web_contents_);
    if (no_state_prefetch_observer)
      tracker->AddObserver(std::move(no_state_prefetch_observer));
#if defined(OS_ANDROID)
    tracker->AddObserver(
        std::make_unique<AndroidPageLoadMetricsObserver>(web_contents_));
#endif  // OS_ANDROID
    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        loading_predictor_observer =
            LoadingPredictorPageLoadMetricsObserver::CreateIfNeeded(
                web_contents_);
    if (loading_predictor_observer)
      tracker->AddObserver(std::move(loading_predictor_observer));
#if !defined(OS_ANDROID)
    tracker->AddObserver(
        std::make_unique<SessionRestorePageLoadMetricsObserver>());
#endif
    tracker->AddObserver(
        std::make_unique<LocalNetworkRequestsPageLoadMetricsObserver>());
  } else {
    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        prerender_observer =
            PrerenderPageLoadMetricsObserver::CreateIfNeeded(web_contents_);
    if (prerender_observer)
      tracker->AddObserver(std::move(prerender_observer));
  }
  tracker->AddObserver(
      std::make_unique<OmniboxSuggestionUsedMetricsObserver>(IsPrerendering()));
  tracker->AddObserver(
      SecurityStatePageLoadMetricsObserver::MaybeCreateForProfile(
          web_contents_->GetBrowserContext()));
}

bool PageLoadMetricsEmbedder::IsPrerendering() const {
  return prerender::PrerenderContents::FromWebContents(web_contents_) !=
         nullptr;
}

std::unique_ptr<base::OneShotTimer> PageLoadMetricsEmbedder::CreateTimer() {
  return std::make_unique<base::OneShotTimer>();
}

bool PageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile)
    return false;
  return search::IsInstantNTPURL(url, profile);
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace chrome
