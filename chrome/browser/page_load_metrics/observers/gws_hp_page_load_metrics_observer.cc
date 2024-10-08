// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_hp_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/named_trigger.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.GoogleHomepage."

const char kHistogramGWSHpParseStart[] =
    HISTOGRAM_PREFIX "ParseTiming.NavigationToParseStart";
const char kHistogramGWSHpConnectStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationToConnectStart";
const char kHistogramGWSHpDomainLookupStart[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupStart";
const char kHistogramGWSHpDomainLookupEnd[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupEnd";
}  // namespace internal

GWSHpPageLoadMetricsObserver::GWSHpPageLoadMetricsObserver() {
  static bool is_first_navigation = true;
  is_first_navigation_ = is_first_navigation;
  is_first_navigation = false;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSHpPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (page_load_metrics::IsGoogleSearchHomepageUrl(
          navigation_handle->GetURL())) {
    // Emit a trigger to allow trace collection tied to gws hp navigations.
    base::trace_event::EmitNamedTrigger("gws-navigation-start-hp");
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSHpPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!page_load_metrics::IsGoogleSearchHomepageUrl(
          navigation_handle->GetURL())) {
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSHpPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSHpPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

void GWSHpPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(AddHistogramSuffix(internal::kHistogramGWSHpParseStart),
                      timing.parse_timing->parse_start.value());
}

void GWSHpPageLoadMetricsObserver::OnConnectStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.connect_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(AddHistogramSuffix(internal::kHistogramGWSHpConnectStart),
                      timing.connect_start.value());
}

void GWSHpPageLoadMetricsObserver::OnDomainLookupStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSHpDomainLookupStart),
      timing.domain_lookup_timing->domain_lookup_start.value());
}

void GWSHpPageLoadMetricsObserver::OnDomainLookupEnd(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_end, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSHpDomainLookupEnd),
      timing.domain_lookup_timing->domain_lookup_end.value());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSHpPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return STOP_OBSERVING;
}

std::string GWSHpPageLoadMetricsObserver::AddHistogramSuffix(
    const std::string& histogram_name) {
  std::string suffix =
      (is_first_navigation_ ? internal::kSuffixFirstNavigation
                            : internal::kSuffixSubsequentNavigation);
  if (!AfterStartupTaskUtils::IsBrowserStartupComplete()) {
    suffix += internal::kSuffixIsBrowserStarting;
  }

  return histogram_name + suffix;
}
