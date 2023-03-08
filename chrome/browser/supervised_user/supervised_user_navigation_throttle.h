// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "content/public/browser/navigation_throttle.h"

class SupervisedUserNavigationThrottle : public content::NavigationThrottle {
 public:
  enum CallbackActions { kCancelNavigation = 0, kCancelWithInterstitial };

  // Returns a new throttle for the given navigation, or nullptr if no
  // throttling is required.
  static std::unique_ptr<SupervisedUserNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* navigation_handle);

  SupervisedUserNavigationThrottle(const SupervisedUserNavigationThrottle&) =
      delete;
  SupervisedUserNavigationThrottle& operator=(
      const SupervisedUserNavigationThrottle&) = delete;

  ~SupervisedUserNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  SupervisedUserNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  // Checks the current URL for the navigation. If called from
  // WillRedirectRequest, checks the URL being redirected to, not the original
  // URL.
  ThrottleCheckResult CheckURL();

  void ShowInterstitial(const GURL& url,
                        supervised_user::FilteringBehaviorReason reason);

  void ShowInterstitialAsync(supervised_user::FilteringBehaviorReason reason);

  void OnCheckDone(
      const GURL& url,
      supervised_user::SupervisedUserURLFilter::FilteringBehavior behavior,
      supervised_user::FilteringBehaviorReason reason,
      bool uncertain);

  void OnInterstitialResult(CallbackActions continue_request,
                            bool already_requested_permission,
                            bool is_main_frame);

  raw_ptr<supervised_user::SupervisedUserURLFilter> url_filter_;
  bool deferred_;
  supervised_user::FilteringBehaviorReason reason_;
  supervised_user::SupervisedUserURLFilter::FilteringBehavior behavior_;
  base::WeakPtrFactory<SupervisedUserNavigationThrottle> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_THROTTLE_H_
