// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace content {
class NavigationHandle;
}  // namespace content

namespace profile_management {

class SAMLResponseParser;

// This throttle looks for profile management data in certain HTTP responses
// from supported hosts. If a response from a supported host is received, the
// navigation is deferred while trying to retrieve the data from the response
// body. If profile management data is found, this throttle triggers signin
// interception.
class ProfileManagementNavigationThrottle : public content::NavigationThrottle {
 public:
  // Create a navigation throttle for the given navigation if third-party
  // profile management is enabled. Returns nullptr if no throttling should be
  // done.
  static std::unique_ptr<ProfileManagementNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* navigation_handle);

  explicit ProfileManagementNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  ProfileManagementNavigationThrottle(
      const ProfileManagementNavigationThrottle&) = delete;
  ProfileManagementNavigationThrottle& operator=(
      const ProfileManagementNavigationThrottle&) = delete;
  ~ProfileManagementNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  // Looks for profile management data in the navigation response if the host is
  // supported.
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  // Setting a non-empty URL causes the throttle to navigate to a test URL
  // instead of triggering profile separation (`token_url`) or resuming the
  // throttle (`unmanaged_url`). Used by unit tests to verify the various code
  // paths taken by the throttle.
  void SetURLsForTesting(const std::string& token_url,
                         const std::string& unmanaged_url);

  // The attribute map is not destructed for performance reasons. This allows
  // the map to be cleared and re-populated on a per-test basis.
  void ClearAttributeMapForTesting();

 private:
  void OnResponseBodyReady(const std::string& body);

  void OnManagementDataReceived(
      const base::flat_map<std::string, std::string>& attributes);

  // `NavigateTo()` can synchronously delete the NavigationThrottle, so
  // `PostNavigateTo()` posts the method to the current thread. This allows code
  // to be safely added after `NavigateTo()` calls.
  void PostNavigateTo(const GURL& url);
  // Don't use directly. Use `PostNavigateTo()` instead.
  void NavigateTo(const GURL& url);

  void RegisterWithDomain(const std::string& domain);
  void RegisterWithToken(const std::string& name, const std::string& token);

  std::string token_url_for_testing_;
  std::string unmanaged_url_for_testing_;
  std::unique_ptr<SAMLResponseParser> saml_response_parser_;
  base::WeakPtrFactory<ProfileManagementNavigationThrottle> weak_ptr_factory_{
      this};
};

}  // namespace profile_management

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_NAVIGATION_THROTTLE_H_
