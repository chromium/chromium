// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_TOKEN_MANAGEMENT_PROFILE_TOKEN_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_TOKEN_MANAGEMENT_PROFILE_TOKEN_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

namespace profile_token_management {

extern const char kTestHost[];

// This throttle looks for an enrollment token in a web page from a supported
// host. If a navigation matches a supported webhost, the navigation is deferred
// while trying to get an enrollment token and profile id. Whether an
// enrollement token is found the navigation continues and a signin interception
// is triggered if an enrollment token is found.
class ProfileTokenNavigationThrottle : public content::NavigationThrottle {
 public:
  class TokenInfoGetter {
   public:
    virtual ~TokenInfoGetter();
    // Gets the profile id and enrollment token from `navigation_handle` and
    // and calls `callback` with them. Both values will be empty strings if no
    // info is found.
    virtual void GetTokenInfo(
        content::NavigationHandle* navigation_handle,
        base::OnceCallback<void(const std::string&, const std::string&)>
            callback) = 0;
  };

  // Create a navigation throttle for the given navigation if device trust is
  // enabled. Returns nullptr if no throttling should be done.
  static std::unique_ptr<ProfileTokenNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  ProfileTokenNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<TokenInfoGetter> token_info_getter);
  ~ProfileTokenNavigationThrottle() override;
  ProfileTokenNavigationThrottle(const ProfileTokenNavigationThrottle&) =
      delete;
  ProfileTokenNavigationThrottle& operator=(
      const ProfileTokenNavigationThrottle&) = delete;

  // content::NavigationThrottle implementation:
  // Looks for an enrollment token in the navigation if the host is supported.
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;

  const char* GetNameForLogging() override;

 private:
  void OnTokenInfoReceived(const std::string& id,
                           const std::string& management_token);
  std::unique_ptr<TokenInfoGetter> token_info_getter_;
  base::WeakPtrFactory<ProfileTokenNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace profile_token_management

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_TOKEN_MANAGEMENT_PROFILE_TOKEN_NAVIGATION_THROTTLE_H_
