// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/delayed_warning_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/navigation_handle.h"

namespace {
const char kConsoleMessage[] =
    "A SafeBrowsing warning is pending on this page, so an attempted download "
    "was cancelled. See https://crbug.com/1081317 for details.";
}

namespace safe_browsing {

DelayedWarningNavigationThrottle::DelayedWarningNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

DelayedWarningNavigationThrottle::~DelayedWarningNavigationThrottle() = default;

const char* DelayedWarningNavigationThrottle::GetNameForLogging() {
  return "DelayedWarningNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DelayedWarningNavigationThrottle::WillProcessResponse() {
  DCHECK(base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings));
  SafeBrowsingUserInteractionObserver* observer =
      SafeBrowsingUserInteractionObserver::FromWebContents(
          navigation_handle()->GetWebContents());
  if (navigation_handle()->IsDownload() && observer) {
    // If the SafeBrowsing interstitial is delayed on the page, ignore
    // downloads. The observer will record the histogram entry for this.
    navigation_handle()
        ->GetWebContents()
        ->GetPrimaryMainFrame()
        ->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              kConsoleMessage);
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

}  // namespace safe_browsing
