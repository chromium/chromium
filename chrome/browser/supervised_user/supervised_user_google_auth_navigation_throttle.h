// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_GOOGLE_AUTH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_GOOGLE_AUTH_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "content/public/browser/navigation_throttle.h"

namespace supervised_user {
class ChildAccountService;
}  // namespace supervised_user

class Profile;

class SupervisedUserGoogleAuthNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Returns a new throttle for the given navigation handle, or nullptr if no
  // throttling is required.
  static std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
  MaybeCreate(content::NavigationHandle* navigation_handle);

  SupervisedUserGoogleAuthNavigationThrottle(
      const SupervisedUserGoogleAuthNavigationThrottle&) = delete;
  SupervisedUserGoogleAuthNavigationThrottle& operator=(
      const SupervisedUserGoogleAuthNavigationThrottle&) = delete;

  ~SupervisedUserGoogleAuthNavigationThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

  void set_skip_jni_call_for_testing(bool is_jni_call_skipped) {
    skip_jni_call_for_testing_ = is_jni_call_skipped;
  }

 private:
  SupervisedUserGoogleAuthNavigationThrottle(
      Profile* profile,
      content::NavigationHandle* navigation_handle);

  void OnGoogleAuthStateChanged();

  ThrottleCheckResult WillStartOrRedirectRequest();

  ThrottleCheckResult ShouldProceed();

  void OnReauthenticationFailed();

  raw_ptr<supervised_user::ChildAccountService> child_account_service_;
  base::CallbackListSubscription google_auth_state_subscription_;

#if BUILDFLAG(IS_ANDROID)
  bool has_shown_reauth_;
#endif

  // Used only for testing to omit the JNI call in ReauthenticateChildAccount().
  bool skip_jni_call_for_testing_ = false;

  base::WeakPtrFactory<SupervisedUserGoogleAuthNavigationThrottle>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_GOOGLE_AUTH_NAVIGATION_THROTTLE_H_
