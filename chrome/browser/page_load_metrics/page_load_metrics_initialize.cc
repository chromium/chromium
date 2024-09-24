// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/bookmark_bar_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_abandoned_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core/amp_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/foreground_duration_ukm_observer.h"
#include "chrome/browser/page_load_metrics/observers/formfill_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/gws_hp_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/javascript_frameworks_ukm_observer.h"
#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/loading_predictor_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/local_network_requests_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/new_tab_page_initiated_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/new_tab_page_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/optimization_guide_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/preview_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/security_state_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/signed_exchange_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/tab_strip_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/third_party_cookie_deprecation_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/translate_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_memory_tracker_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/third_party_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/zstd_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/page_load_metrics/observers/ash_session_restore_page_load_metrics_observer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/page_load_metrics/observers/side_search_page_load_metrics_observer.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#endif  // defined(TOOLKIT_VIEWS)

namespace {

#if !BUILDFLAG(IS_ANDROID)
bool IsNonTabWebUI(content::WebContents* web_contents) {
  return !!TopChromeWebUIConfig::From(web_contents->GetBrowserContext(),
                                      web_contents->GetVisibleURL());
}

std::string GetNonTabWebUIName(content::WebContents* web_contents) {
  CHECK(IsNonTabWebUI(web_contents));
  return TopChromeWebUIConfig::From(web_contents->GetBrowserContext(),
                                    web_contents->GetVisibleURL())
      ->GetWebUIName();
}
#else
bool IsNonTabWebUI(content::WebContents* web_contents) {
  return false;
}
#endif

}  // namespace

namespace chrome {

namespace {

std::string GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents);

  PageLoadMetricsEmbedder(const PageLoadMetricsEmbedder&) = delete;
  PageLoadMetricsEmbedder& operator=(const PageLoadMetricsEmbedder&) = delete;

  ~PageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override;
  bool IsNoStatePrefetch(content::WebContents* web_contents) override;
  bool IsExtensionUrl(const GURL& url) override;
  bool IsSidePanel(content::WebContents* web_contents) override;
  bool IsNonTabWebUI() override;
  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override;

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker,
                         content::NavigationHandle* navigation_handle) override;
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : PageLoadMetricsEmbedderBase(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() = default;

void PageLoadMetricsEmbedder::RegisterObservers(
    page_load_metrics::PageLoadTracker* tracker,
    content::NavigationHandle* navigation_handle) {
  // Page-specific observers.
  //
  // Other observers don't get installed because they measure things that
  // don't apply to this type of page, rely on invariants that aren't true
  // about WebUI chrome pages (such as visibility-related things),
  // or because they depend on objects that don't exist for WebUI pages
  // (namely `TabHelper`s).

  // TODO(crbug.com/40823327): Integrate side panel metrics with UKM.
  if (IsSidePanel(web_contents())) {
#if defined(TOOLKIT_VIEWS)
    if (auto side_search_observer =
            SideSearchPageLoadMetricsObserver::CreateIfNeeded(
                tracker->GetWebContents())) {
      tracker->AddObserver(std::move(side_search_observer));
    }
#endif  // defined(TOOLKIT_VIEWS)
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (IsNonTabWebUI()) {
    // This embedder is for a non-tab chrome:// page.
    tracker->AddObserver(std::make_unique<NonTabPageLoadMetricsObserver>(
        GetNonTabWebUIName(web_contents())));
    return;
  }
#endif

  if (IsNewTabPageUrl(navigation_handle->GetURL())) {
    tracker->AddObserver(std::make_unique<NewTabPagePageLoadMetricsObserver>());
    return;
  }

  // Observers for regular web pages.
  RegisterCommonObservers(tracker);

  if (!IsNoStatePrefetch(web_contents())) {
    tracker->AddObserver(std::make_unique<AMPPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<JavascriptFrameworksUkmObserver>());
    tracker->AddObserver(std::make_unique<SchemePageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<FromGWSPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<GWSPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<AbandonedPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<ChromeGWSAbandonedPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<GWSHpPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<ForegroundDurationUKMObserver>());
    tracker->AddObserver(
        std::make_unique<DocumentWritePageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<PrefetchPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<MultiTabLoadingPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<OptimizationGuidePageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<ServiceWorkerPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<SignedExchangePageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<HttpsEngagementPageLoadMetricsObserver>(
            web_contents()->GetBrowserContext()));
    tracker->AddObserver(std::make_unique<ProtocolPageLoadMetricsObserver>());
    std::unique_ptr<page_load_metrics::AdsPageLoadMetricsObserver>
        ads_observer =
            page_load_metrics::AdsPageLoadMetricsObserver::CreateIfNeeded(
                tracker->GetWebContents(),
                HeavyAdServiceFactory::GetForBrowserContext(
                    tracker->GetWebContents()->GetBrowserContext()),
                base::BindRepeating(&GetApplicationLocale));
    if (ads_observer)
      tracker->AddObserver(std::move(ads_observer));

    tracker->AddObserver(std::make_unique<ThirdPartyMetricsObserver>());
    tracker->AddObserver(std::make_unique<FormfillPageLoadMetricsObserver>());

    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver> ukm_observer =
        UkmPageLoadMetricsObserver::CreateIfNeeded();
    if (ukm_observer)
      tracker->AddObserver(std::move(ukm_observer));

#if BUILDFLAG(IS_ANDROID)
    tracker->AddObserver(std::make_unique<AndroidPageLoadMetricsObserver>());
#endif  // BUILDFLAG(IS_ANDROID)
    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        loading_predictor_observer =
            LoadingPredictorPageLoadMetricsObserver::CreateIfNeeded(
                web_contents());
    if (loading_predictor_observer)
      tracker->AddObserver(std::move(loading_predictor_observer));
    if (blink::LcppEnabled()) {
      tracker->AddObserver(
          std::make_unique<LcpCriticalPathPredictorPageLoadMetricsObserver>());
    }
    tracker->AddObserver(
        std::make_unique<LocalNetworkRequestsPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<TabStripPageLoadMetricsObserver>(web_contents()));
    tracker->AddObserver(std::make_unique<PreviewPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<BookmarkBarMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<NewTabPageInitiatedPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<ThirdPartyCookieDeprecationMetricsObserver>(
            web_contents()->GetBrowserContext()));
  }
  tracker->AddObserver(
      std::make_unique<OmniboxSuggestionUsedMetricsObserver>());
  tracker->AddObserver(
      SecurityStatePageLoadMetricsObserver::MaybeCreateForProfile(
          web_contents()->GetBrowserContext()));
  tracker->AddObserver(
      std::make_unique<PageAnchorsMetricsObserver>(tracker->GetWebContents()));
  std::unique_ptr<TranslatePageLoadMetricsObserver> translate_observer =
      TranslatePageLoadMetricsObserver::CreateIfNeeded(
          tracker->GetWebContents());
  if (translate_observer)
    tracker->AddObserver(std::move(translate_observer));
  tracker->AddObserver(std::make_unique<ZstdPageLoadMetricsObserver>());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (AshSessionRestorePageLoadMetricsObserver::ShouldBeInstantiated(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()))) {
    tracker->AddObserver(
        std::make_unique<AshSessionRestorePageLoadMetricsObserver>(
            web_contents()));
  }
#endif
}

bool PageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile)
    return false;
  return search::IsInstantNTPURL(url, profile);
}

bool PageLoadMetricsEmbedder::IsNoStatePrefetch(
    content::WebContents* web_contents) {
  return prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
      web_contents);
}

bool PageLoadMetricsEmbedder::IsExtensionUrl(const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return url.SchemeIs(extensions::kExtensionScheme);
#else
  return false;
#endif
}

bool PageLoadMetricsEmbedder::IsSidePanel(content::WebContents* web_contents) {
#if defined(TOOLKIT_VIEWS)
  return side_search::IsSidePanelWebContents(web_contents);
#else
  return false;
#endif  // defined(TOOLKIT_VIEWS)
}

bool PageLoadMetricsEmbedder::IsNonTabWebUI() {
  return ::IsNonTabWebUI(web_contents());
}

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring))
    return nullptr;

  return page_load_metrics::PageLoadMetricsMemoryTrackerFactory::
      GetForBrowserContext(browser_context);
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  // Change this method? consider to modify the peer in
  // android_webview/browser/page_load_metrics/page_load_metrics_initialize.cc
  // weblayer/browser/page_load_metrics_initialize.cc
  // as well.
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace chrome
