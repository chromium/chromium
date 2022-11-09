// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_MERGE_SESSION_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_MERGE_SESSION_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

namespace ash {

// Used to delay a navigation while merge session process (cookie
// reconstruction from OAuth2 refresh token in ChromeOS login) is still in
// progress while we are attempting to load a google property. It will resume
// the navigation once merge session is done, or after 10 seconds.
class MergeSessionNavigationThrottle : public content::NavigationThrottle,
                                       public OAuth2LoginManager::Observer {
 public:
  static std::unique_ptr<content::NavigationThrottle> Create(
      content::NavigationHandle* handle);

  MergeSessionNavigationThrottle(const MergeSessionNavigationThrottle&) =
      delete;
  MergeSessionNavigationThrottle& operator=(
      const MergeSessionNavigationThrottle&) = delete;

  ~MergeSessionNavigationThrottle() override;

 private:
  explicit MergeSessionNavigationThrottle(content::NavigationHandle* handle);

  // content::NavigationThrottle implementation:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  const char* GetNameForLogging() override;

  // OAuth2LoginManager::Observer implementation:
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override;

  // Sets up timer and OAuth2LoginManager Observer, should be called before
  // deferring. Returns true if the Observer was set up correctly and the
  // navigation still needs to be delayed, false otherwise.
  bool BeforeDefer();

  // Cleans up timer and OAuth2LoginManager Observer, then resumes a deferred
  // navigation.
  void Proceed();

  base::ScopedObservation<OAuth2LoginManager, OAuth2LoginManager::Observer>
      login_manager_observation_{this};

  base::OneShotTimer proceed_timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_MERGE_SESSION_NAVIGATION_THROTTLE_H_
