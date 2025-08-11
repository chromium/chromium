// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/serp_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

SerpPageLoadMetricsObserver::SerpPageLoadMetricsObserver() = default;

SerpPageLoadMetricsObserver::~SerpPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SerpPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // A user-initiated search is expected to be in the foreground.
  if (!started_in_foreground) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SerpPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SerpPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SerpPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  if (!profile) {
    return STOP_OBSERVING;
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return STOP_OBSERVING;
  }

  bool is_dse_serp =
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          navigation_handle->GetURL());
  base::UmaHistogramBoolean("PageLoad.CommittedPageIsDseSerp", is_dse_serp);

  if (!is_dse_serp) {
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

void SerpPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  Profile* profile = Profile::FromBrowserContext(
      GetDelegate().GetWebContents()->GetBrowserContext());
  if (!profile) {
    return;
  }

  safe_browsing::ExtensionTelemetryService* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(profile);
  if (telemetry_service) {
    telemetry_service->OnDseSerpLoaded();
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SerpPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Stop observing if the page is hidden. We only care about SERP landings that
  // are visible to the user.
  return STOP_OBSERVING;
}
