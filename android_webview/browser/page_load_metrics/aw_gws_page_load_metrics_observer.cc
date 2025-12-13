// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_gws_page_load_metrics_observer.h"

namespace android_webview {

AwGWSPageLoadMetricsObserver::AwGWSPageLoadMetricsObserver() = default;

bool AwGWSPageLoadMetricsObserver::IsFromNewTabPage(
    content::NavigationHandle* navigation_handle) {
  return false;
}

bool AwGWSPageLoadMetricsObserver::IsBrowserStartupComplete() {
  return true;
}

bool AwGWSPageLoadMetricsObserver::IsIncognitoProfile() const {
  // Always returns false since WebView does not have Incognito mode.
  return false;
}

bool AwGWSPageLoadMetricsObserver::IsSignedIn(
    content::BrowserContext* browser_context) const {
  // Not implemented. Always returns false.
  return false;
}

content::BrowserContext*
AwGWSPageLoadMetricsObserver::GetOriginalBrowserContext() {
  return nullptr;
}

}  // namespace android_webview
