// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/top_chrome_webui_metrics_observer.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/common/url_constants.h"

namespace {

std::string GetMetricName(const std::string& webui_name,
                          const std::string& metric_name) {
  return base::StrCat({"TopChromeUI.", webui_name, ".", metric_name});
}

// This duplicates logic from
// chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.cc
// TODO(crbug.com/491337216): Refactor this to a common location.
base::TimeDelta GetBackgroundTime(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  const std::optional<base::TimeTicks> request_time =
      WebUIContentsPreloadManager::GetInstance()->GetRequestTime(
          delegate.GetWebContents());
  if (!request_time.has_value()) {
    // The WebUIContentsPreloadManager may not have a record of when the user
    // opened the WebUI. This may happen in unit tests, or if a non-tab WebUI
    // is opened in a tab for debugging purposes. In these cases, we define the
    // "background time" to be zero.
    return base::TimeDelta();
  }

  const base::TimeTicks last_navigation_time = delegate.GetNavigationStart();
  // The request time is earlier than the last navigation time if the WebUI
  // refreshes or redirects. In this case the WebUI is never in the background
  // since last navigation.
  const base::TimeDelta background_time =
      std::max(*request_time - last_navigation_time, base::TimeDelta());
  return background_time;
}

}  // namespace

TopChromeWebUIMetricsObserver::TopChromeWebUIMetricsObserver(
    std::string webui_name)
    : webui_name_(std::move(webui_name)) {}

TopChromeWebUIMetricsObserver::~TopChromeWebUIMetricsObserver() = default;

void TopChromeWebUIMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint.has_value());

  // Time from request to FCP. This metric disregards time spent in the
  // background, which is non-zero when the WebUI is preloaded.
  base::TimeDelta first_contentful_paint =
      timing.paint_timing->first_contentful_paint.value();
  base::TimeDelta background_time = GetBackgroundTime(GetDelegate());
  PAGE_LOAD_SHORT_HISTOGRAM(
      GetMetricName(webui_name_, "RequestToFirstContentfulPaint"),
      first_contentful_paint - background_time);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TopChromeWebUIMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TopChromeWebUIMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TopChromeWebUIMetricsObserver::ShouldObserveScheme(const GURL& url) const {
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

// static
void TopChromeWebUIMetricsObserver::RecordFirstContentfulPaint(
    const std::string& webui_name,
    base::TimeDelta duration) {
  PAGE_LOAD_SHORT_HISTOGRAM(
      GetMetricName(webui_name, "RequestToFirstContentfulPaint"), duration);
}
