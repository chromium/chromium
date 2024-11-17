// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/chrome_gws_page_load_metrics_observer.h"

#include <string>

#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"

ChromeGWSPageLoadMetricsObserver::ChromeGWSPageLoadMetricsObserver() = default;

bool ChromeGWSPageLoadMetricsObserver::IsFromNewTabPage(
    content::NavigationHandle* navigation_handle) {
  auto* start_instance = navigation_handle->GetStartingSiteInstance();
  if (!start_instance) {
    return false;
  }

  auto origin = start_instance->GetSiteURL();

  GURL ntp_url(chrome::kChromeUINewTabPageURL);
  return ntp_url.scheme_piece() == origin.scheme_piece() &&
         ntp_url.host_piece() == origin.host_piece();
}

bool ChromeGWSPageLoadMetricsObserver::IsBrowserStartupComplete() {
  return AfterStartupTaskUtils::IsBrowserStartupComplete();
}
