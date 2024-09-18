// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/chrome_gws_abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace internal {

const char kSuffixResponseFromCache[] = ".ResponseFromCache";
const char kSuffixRTTBelow200[] = ".RTTBelow200";
const char kSuffixRTT200to450[] = ".RTT200To450";
const char kSuffixRTTAbove450[] = ".RTTAbove450";
const char kIncognito[] = ".Incognito";

}  // namespace internal

const char* ChromeGWSAbandonedPageLoadMetricsObserver::GetSuffixForRTT(
    std::optional<base::TimeDelta> rtt) {
  if (rtt.value().InMilliseconds() < 200) {
    return internal::kSuffixRTTBelow200;
  }
  if (rtt.value().InMilliseconds() <= 450) {
    return internal::kSuffixRTT200to450;
  }

  return internal::kSuffixRTTAbove450;
}

ChromeGWSAbandonedPageLoadMetricsObserver::
    ChromeGWSAbandonedPageLoadMetricsObserver() = default;

ChromeGWSAbandonedPageLoadMetricsObserver::
    ~ChromeGWSAbandonedPageLoadMetricsObserver() = default;

std::vector<std::string>
ChromeGWSAbandonedPageLoadMetricsObserver::GetAdditionalSuffixes() const {
  std::vector<std::string> suffixes;
  // Add the incognito suffix if the current profile is incognito mode.
  for (std::string& suffix :
       GWSAbandonedPageLoadMetricsObserver::GetAdditionalSuffixes()) {
    suffixes.push_back(suffix);
    if (IsIncognitoProfile()) {
      suffixes.push_back(suffix + internal::kIncognito);
    }
  }
  // Make sure each histogram logged will log a version without connection type,
  // and a version with the connection type, to allow filtering if needed.
  // TODO(https://crbug.com/347706997): Consider doing this for the WebView
  // version as well.
  std::vector<std::string> suffixes_with_rtt;
  for (std::string& base_suffix : suffixes) {
    suffixes_with_rtt.push_back(base_suffix);
    if (IsResponseFromCache()) {
      suffixes_with_rtt.push_back(base_suffix +
                                  internal::kSuffixResponseFromCache);
    } else {
      std::optional<base::TimeDelta> rtt =
          g_browser_process->network_quality_tracker()->GetHttpRTT();
      if (rtt.has_value()) {
        suffixes_with_rtt.push_back(base_suffix + GetSuffixForRTT(rtt));
      }
    }
  }
  return suffixes_with_rtt;
}

void ChromeGWSAbandonedPageLoadMetricsObserver::AddSRPMetricsToUKMIfNeeded(
    ukm::builders::AbandonedSRPNavigation& builder) {
  GWSAbandonedPageLoadMetricsObserver::AddSRPMetricsToUKMIfNeeded(builder);
  std::optional<base::TimeDelta> rtt =
      g_browser_process->network_quality_tracker()->GetHttpRTT();
  if (rtt.has_value()) {
    builder.SetRTT(ukm::GetSemanticBucketMinForDurationTiming(
        rtt.value().InMilliseconds()));
  }
}

bool ChromeGWSAbandonedPageLoadMetricsObserver::IsIncognitoProfile() const {
  if (Profile* profile = Profile::FromBrowserContext(
          GetDelegate().GetWebContents()->GetBrowserContext())) {
    return profile->IsIncognitoProfile();
  }
  return false;
}
