// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CLASSIFY_URL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CLASSIFY_URL_NAVIGATION_THROTTLE_H_

#include <list>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace supervised_user {

// Returns a new throttle for the given navigation, or nullptr if no
// throttling is required.
std::unique_ptr<content::NavigationThrottle>
MaybeCreateClassifyUrlNavigationThrottleFor(
    content::NavigationHandle* navigation_handle);

// Navigation throttle that processes requests and redirects in parallel with
// their verification against ClassifyUrl, up until the response is ready for
// processing. Only then the navigation can be deferred.
class ClassifyUrlNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<ClassifyUrlNavigationThrottle> MakeUnique(
      content::NavigationHandle* navigation_handle);

  ClassifyUrlNavigationThrottle(const ClassifyUrlNavigationThrottle&) = delete;
  ClassifyUrlNavigationThrottle& operator=(
      const ClassifyUrlNavigationThrottle&) = delete;

  ~ClassifyUrlNavigationThrottle() override;

 private:
  explicit ClassifyUrlNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  // Named tuple for bits of result.
  struct CheckResult {
    FilteringBehavior behavior;
    FilteringBehaviorReason reason;
  };

  // Represents a check. For checks in flight, result is nullopt.
  struct Check {
    GURL url;
    std::optional<CheckResult> result;
  };

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  // Common procedure for both initial request and redirects.
  ThrottleCheckResult WillProcessRequest();

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  const GURL& currently_navigated_url() const;

  // Verdict is decided if either:
  // * all checks resulted in kAllow
  // * a prefix of checks resulted in kAllow followed by a kBlock
  // Verdict is not decided otherwise, if there's a kInvalid not preceded by
  // kBlock

  // Examples:
  // * kAllow, kAllow, kAllow is decided,
  // * kAllow, kAllow, kBlock is decided,
  // * kAllow, kBlock, kInvalid is decided,
  // * kAllow, kAllow, kInvalid is not decided (might be allow or block),
  // * kAllow, kInvalid, kBlock is not decided (unsure what redirect is a
  // block).
  bool IsDecided() const;

  // Iterator pointing at first blocking verdict if it's defined, or ::cend()
  // otherwise.
  std::list<Check>::const_iterator FirstBlockingCheck() const;

  // Updates the pending_check item with matching check (by url field).
  void SetCheck(const GURL& url, const CheckResult& result);

  // Triggers a URL check; the result might be processed either synchronously
  // or asynchronously.
  void CheckURL();
  void OnURLCheckDone(const GURL& url,
                      FilteringBehavior behavior,
                      FilteringBehaviorReason reason,
                      bool uncertain);

  // Interstitial handling
  void ScheduleInterstitial(Check check);
  void ShowInterstitial(Check check);
  void OnInterstitialResult(
      Check check,
      SupervisedUserNavigationThrottle::CallbackActions action,

      bool already_sent_request,
      bool is_main_frame);

  // Ordered list of pending and completed checks.
  std::list<Check> checks_;

  // True iff one of navigation events returned ::DEFER.
  bool deferred_{false};

  // Timers forming a continuum of time, only recorded in unblocked navigation
  // (success) case.
  std::optional<base::ElapsedTimer> waiting_for_decision_;
  std::optional<base::ElapsedTimer> waiting_for_process_response_;

  raw_ptr<supervised_user::SupervisedUserURLFilter> url_filter_;
  base::WeakPtrFactory<ClassifyUrlNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_CLASSIFY_URL_NAVIGATION_THROTTLE_H_
