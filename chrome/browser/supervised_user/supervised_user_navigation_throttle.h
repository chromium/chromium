// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

class Profile;

namespace supervised_user {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
bool ShouldShowReAuthInterstitial(content::NavigationHandle& navigation_handle,
                                  bool is_main_frame);
#endif
}  // namespace supervised_user

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
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  SupervisedUserNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  // Triggers the checks of the current URL for the navigation. If called from
  // WillRedirectRequest, checks the URL being redirected to, not the original
  // URL.
  void CheckURL();

  // Wraps up common procedure for throttling new requests or redirects.
  ThrottleCheckResult ProcessRequest();

  void ShowInterstitial(const GURL& url,
                        supervised_user::FilteringBehaviorReason reason);

  void ShowInterstitialAsync(supervised_user::FilteringBehaviorReason reason);

  void OnCheckDone(const GURL& url,
                   supervised_user::FilteringBehavior behavior,
                   supervised_user::FilteringBehaviorReason reason,
                   bool uncertain);

  void OnInterstitialResult(CallbackActions continue_request,
                            bool already_requested_permission,
                            bool is_main_frame);

  raw_ptr<supervised_user::SupervisedUserURLFilter> url_filter_;
  bool deferred_;
  supervised_user::FilteringBehaviorReason reason_;
  supervised_user::FilteringBehavior behavior_;

  // See the ParallelNavigationThrottle. Since this throttle is always deferring
  // navigation if check is asynchronous, it can only end up in 0 or positive
  // delay.
  std::optional<base::ElapsedTimer> waiting_for_decision_;
  base::TimeDelta total_delay_;

  base::WeakPtrFactory<SupervisedUserNavigationThrottle> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_THROTTLE_H_
