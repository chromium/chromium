// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_VIEW_SOURCE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_VIEW_SOURCE_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "content/public/browser/navigation_throttle.h"

namespace enterprise_data_protection {

// This NavigationThrottle is responsible for checking `view-source:`
// navigations against the Safe Browsing Enterprise policy.
//
// The standard Safe Browsing check, which occurs in
// `BrowserURLLoaderThrottle`, is not sufficient for this purpose.
// `BrowserURLLoaderThrottle` operates on network requests, but the
// `view-source:` prefix is a renderer-level instruction and is stripped from
// the URL before the network request is made.
//
// As a `NavigationThrottle`, this class can access the `NavigationHandle`,
// which provides the complete virtual URL (via
// `NavigationHandle::GetVirtualURL()`). This allows it to inspect the full URL,
// including the prefix, and block the navigation if it is disallowed by policy.
class ViewSourceNavigationThrottle : public content::NavigationThrottle {
 public:
  // Static factory method to create and add this throttle to a navigation.
  // This throttle is only added if a `SafeBrowsingUIManager` is present,
  // the navigation is for `view-source:`, and it's occurring in a
  // primary or prerendered main frame.
  static void MaybeCreateAndAdd(
      content::NavigationThrottleRegistry& registry,
      safe_browsing::SafeBrowsingUIManager* ui_manager);

  ~ViewSourceNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  ViewSourceNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      safe_browsing::SafeBrowsingUIManager* ui_manager);

  void OnRealTimeLookupComplete(
      content::NavigationHandle* handle,
      bool is_success,
      bool is_cached,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  content::NavigationThrottle::ThrottleCheckResult FireRealtimeLookup();

  base::WeakPtr<safe_browsing::RealTimeUrlLookupServiceBase>
      url_lookup_service_ = nullptr;
  raw_ptr<safe_browsing::SafeBrowsingUIManager> manager_;

  base::WeakPtrFactory<ViewSourceNavigationThrottle> weak_factory_{this};
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_VIEW_SOURCE_NAVIGATION_THROTTLE_H_
