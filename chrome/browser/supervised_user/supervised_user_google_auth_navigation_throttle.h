// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_GOOGLE_AUTH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_GOOGLE_AUTH_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "content/public/browser/navigation_throttle.h"

class ChildAccountService;
class Profile;

class SupervisedUserGoogleAuthNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Returns a new throttle for the given navigation handle, or nullptr if no
  // throttling is required.
  static std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
  MaybeCreate(content::NavigationHandle* navigation_handle);

  ~SupervisedUserGoogleAuthNavigationThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  SupervisedUserGoogleAuthNavigationThrottle(
      Profile* profile,
      content::NavigationHandle* navigation_handle);

  void OnGoogleAuthStateChanged();

  ThrottleCheckResult WillStartOrRedirectRequest();

  ThrottleCheckResult ShouldProceed();

  void OnReauthenticationResult(bool reauth_successful);

  ChildAccountService* child_account_service_;
  std::unique_ptr<base::CallbackList<void()>::Subscription>
      google_auth_state_subscription_;

#if defined(OS_ANDROID)
  bool has_shown_reauth_;
#endif

  base::WeakPtrFactory<SupervisedUserGoogleAuthNavigationThrottle>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserGoogleAuthNavigationThrottle);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_GOOGLE_AUTH_NAVIGATION_THROTTLE_H_
