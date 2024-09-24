// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/page_load_metrics_initialize.h"

#include "android_webview/browser/page_load_metrics/aw_page_load_metrics_memory_tracker_factory.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/third_party_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_load_metrics {
class PageLoadMetricsMemoryTracker;
}  // namespace page_load_metrics

namespace android_webview {

namespace {

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
  RegisterCommonObservers(tracker);
  tracker->AddObserver(std::make_unique<ThirdPartyMetricsObserver>());
  tracker->AddObserver(std::make_unique<AbandonedPageLoadMetricsObserver>());
  tracker->AddObserver(std::make_unique<GWSAbandonedPageLoadMetricsObserver>());
}

bool PageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  return false;
}

bool PageLoadMetricsEmbedder::IsNoStatePrefetch(
    content::WebContents* web_contents) {
  return false;
}

bool PageLoadMetricsEmbedder::IsExtensionUrl(const GURL& url) {
  return false;
}

bool PageLoadMetricsEmbedder::IsSidePanel(content::WebContents* web_contents) {
  // The side panel is not supported on Android so this always returns false.
  return false;
}

bool PageLoadMetricsEmbedder::IsNonTabWebUI() {
  // Android web view doesn't have non-tab webUI surfaces (such as desktop tab
  // search, side panel, etc).
  return false;
}

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring))
    return nullptr;

  return AwPageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
      browser_context);
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  // Change this method? consider to modify the peer in
  // chrome/browser/page_load_metrics/page_load_metrics_initialize.cc
  // weblayer/browser/page_load_metrics_initialize.cc
  // as well.
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace android_webview
