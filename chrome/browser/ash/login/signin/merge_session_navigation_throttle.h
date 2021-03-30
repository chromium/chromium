// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_MERGE_SESSION_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_MERGE_SESSION_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

// Used to delay a navigation while merge session process (cookie
// reconstruction from OAuth2 refresh token in ChromeOS login) is still in
// progress while we are attempting to load a google property. It will resume
// the navigation once merge session is done, or after 10 seconds.
class MergeSessionNavigationThrottle
    : public content::NavigationThrottle,
      public chromeos::OAuth2LoginManager::Observer {
 public:
  static std::unique_ptr<content::NavigationThrottle> Create(
      content::NavigationHandle* handle);
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
      chromeos::OAuth2LoginManager::SessionRestoreState state) override;

  // Sets up timer and OAuth2LoginManager Observer, should be called before
  // deferring. Returns true if the Observer was set up correctly and the
  // navigation still needs to be delayed, false otherwise.
  bool BeforeDefer();

  // Cleans up timer and OAuth2LoginManager Observer, then resumes a deferred
  // navigation.
  void Proceed();

  ScopedObserver<chromeos::OAuth2LoginManager,
                 chromeos::OAuth2LoginManager::Observer>
      login_manager_observer_;

  base::OneShotTimer proceed_timer_;

  DISALLOW_COPY_AND_ASSIGN(MergeSessionNavigationThrottle);
};

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_MERGE_SESSION_NAVIGATION_THROTTLE_H_
