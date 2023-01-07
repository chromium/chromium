// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_navigation_throttle.h"

#include "base/metrics/histogram_functions.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_request_headers.h"

namespace offline_pages {

const char kOfflinePagesDidNavigationThrottleCancelNavigation[] =
    "OfflinePages.DidNavigationThrottleCancelNavigation";

namespace {
void RecordWhetherNavigationWasCanceled(bool was_navigation_canceled) {
  base::UmaHistogramBoolean(kOfflinePagesDidNavigationThrottleCancelNavigation,
                            was_navigation_canceled);
}
}  // namespace

OfflinePageNavigationThrottle::OfflinePageNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

OfflinePageNavigationThrottle::~OfflinePageNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
OfflinePageNavigationThrottle::WillStartRequest() {
  return CANCEL_AND_IGNORE;
}

const char* OfflinePageNavigationThrottle::GetNameForLogging() {
  return "OfflinePageNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  // Checks if the request was initiated by the renderer and if it contains the
  // "X-Chrome-offline" header. There is no legitimate reason for a renderer to
  // make a request with this header, so we can cancel these types of requests.
  if (navigation_handle->IsRendererInitiated() &&
      navigation_handle->GetRequestHeaders().HasHeader(kOfflinePageHeader)) {
    RecordWhetherNavigationWasCanceled(true);
    return std::make_unique<OfflinePageNavigationThrottle>(navigation_handle);
  }
  RecordWhetherNavigationWasCanceled(false);
  return nullptr;
}

}  // namespace offline_pages
