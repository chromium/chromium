// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace internal {

const char kGWSAbandonedPageLoadMetricsHistogramPrefix[] =
    "PageLoad.Clients.GoogleSearch.Leakage2.";
const char kSuffixWasNonSRP[] = ".WasNonSRP";

const char kSuffixRTTUnknown[] = ".RTTUnkown";
const char kSuffixRTTBelow200[] = ".RTTBelow200";
const char kSuffixRTT200to450[] = ".RTT200To450";
const char kSuffixRTTAbove450[] = ".RTTAbove450";

}  // namespace internal

const char* GWSAbandonedPageLoadMetricsObserver::GetSuffixForRTT(
    std::optional<base::TimeDelta> rtt) {
  if (!rtt.has_value()) {
    return internal::kSuffixRTTUnknown;
  }
  if (rtt.value().InMilliseconds() < 200) {
    return internal::kSuffixRTTBelow200;
  }
  if (rtt.value().InMilliseconds() <= 450) {
    return internal::kSuffixRTT200to450;
  }

  return internal::kSuffixRTTAbove450;
}

GWSAbandonedPageLoadMetricsObserver::GWSAbandonedPageLoadMetricsObserver() =
    default;

GWSAbandonedPageLoadMetricsObserver::~GWSAbandonedPageLoadMetricsObserver() =
    default;

const char* GWSAbandonedPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "GWSAbandonedPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnNavigationEvent(
    content::NavigationHandle* navigation_handle) {
  if (page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL())) {
    involved_srp_url_ = true;
  } else {
    did_request_non_srp_ = true;

    if (!navigation_handle->GetNavigationHandleTiming()
             .non_redirect_response_start_time.is_null()) {
      // The navigation has received its final response, meaning that it can't
      // be redirected to SRP anymore, and the current URL is not SRP. As the
      // navigation didn't end up going to SRP, we shouldn't log any metric.
      return STOP_OBSERVING;
    }
  }

  return CONTINUE_OBSERVING;
}

const base::flat_map<std::string,
                     AbandonedPageLoadMetricsObserver::NavigationMilestone>&
GWSAbandonedPageLoadMetricsObserver::GetCustomUserTimingMarkNames() const {
  static const base::NoDestructor<
      base::flat_map<std::string, NavigationMilestone>>
      mark_names(
          {{internal::kGwsAFTStartMarkName, NavigationMilestone::kAFTStart},
           {internal::kGwsAFTEndMarkName, NavigationMilestone::kAFTEnd}});
  return *mark_names;
}

bool GWSAbandonedPageLoadMetricsObserver::IsAllowedToLogMetrics() const {
  // Only log metrics for navigations that involve SRP.
  return involved_srp_url_;
}

bool GWSAbandonedPageLoadMetricsObserver::IsAllowedToLogUKM() const {
  // Only log UKMs for navigations that involve SRP.
  return involved_srp_url_;
}

std::string GWSAbandonedPageLoadMetricsObserver::GetHistogramPrefix() const {
  // Use the GWS-specific histograms.
  return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix;
}

std::vector<std::string>
GWSAbandonedPageLoadMetricsObserver::GetAdditionalSuffixes() const {
  // Add suffix that indicates the navigation prevviously requested a non-SRP
  // URL (instead of immediately targeting a SRP URL) to all histograms, if
  // necessary.
  std::string suffix = did_request_non_srp_ ? internal::kSuffixWasNonSRP : "";
  // Make sure each histogram logged will log a version without connection type,
  // and a version with the connection type, to allow filtering if needed.
  // TODO(https://crbug.com/347706997): Consider doing this for the WebView
  // version as well.
  return {
      suffix,
      suffix + GetSuffixForRTT(
                   g_browser_process->network_quality_tracker()->GetHttpRTT())};
}

void GWSAbandonedPageLoadMetricsObserver::AddSRPMetricsToUKMIfNeeded(
    ukm::builders::AbandonedSRPNavigation& builder) {
  std::optional<base::TimeDelta> rtt =
      g_browser_process->network_quality_tracker()->GetHttpRTT();
  if (rtt.has_value()) {
    builder.SetRTT(ukm::GetSemanticBucketMinForDurationTiming(
        rtt.value().InMilliseconds()));
  }
  builder.SetDidRequestNonSRP(did_request_non_srp_);
}
