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
const char kIncognito[] = ".Incognito";

}  // namespace internal

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
  std::vector<std::string> suffixes_from_cache;
  for (std::string& base_suffix : suffixes) {
    suffixes_from_cache.push_back(base_suffix);
    if (IsResponseFromCache()) {
      suffixes_from_cache.push_back(base_suffix +
                                    internal::kSuffixResponseFromCache);
    }
  }
  return suffixes_from_cache;
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
