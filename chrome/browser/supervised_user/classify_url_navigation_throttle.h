// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CLASSIFY_URL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CLASSIFY_URL_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace supervised_user {

// LINT.IfChange(ClassifyUrlThrottleStatus)
enum class ClassifyUrlThrottleStatus : int {
  kContinue = 0,
  kProceed = 1,
  kDefer = 2,
  kDeferAndScheduleInterstitial = 3,
  kCancel = 4,
  kResume = 5,
  kCancelDeferredNavigation = 6,

  kMaxValue = kCancelDeferredNavigation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:ClassifyUrlThrottleStatus)

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

 protected:
  void CancelDeferredNavigation(ThrottleCheckResult result) override;

 private:
  // Smart container that manages list of pending checks and derives overall
  // verdict for the throttle. The checks are stored in the order in which they
  // were scheduled. The list can be sealed (marked that no more checks will be
  // scheduled) which is important to determine the final verdict.
  class ClassifyUrlCheckList {
   public:
    // Named tuple for bits of result.
    struct FilteringResult {
      GURL url;
      FilteringBehavior behavior;
      FilteringBehaviorReason reason;
    };
    using Key = std::vector<FilteringResult>::size_type;

    ClassifyUrlCheckList();
    ClassifyUrlCheckList(ClassifyUrlCheckList& other) = delete;
    const ClassifyUrlCheckList& operator=(ClassifyUrlCheckList& other) = delete;
    ~ClassifyUrlCheckList();

    // Registers new check if the list is not sealed.
    Key NewCheck();
    void UpdateCheck(Key key, FilteringResult result);

    // Returns blocking Filtering result if there's one or nothing.
    std::optional<FilteringResult> GetBlockingResult() const;

    // Returns true if this classification allowed or blocking.
    bool IsDecided() const;

    // Seals this instance. After calling this method `NewCheck` cannot be
    // called, but behavior of `IsDecided()` changes.
    void MarkNavigationRequestsCompleted();

    base::TimeDelta ElapsedSinceDecided() const;

   private:
    std::vector<std::optional<FilteringResult>> results_;

    // After disabling new checks can't be issued, but it enables positive
    // verification of all-allow results.
    bool new_checks_disabled_{false};

    // Tracks time from being updated by the most recent result that had effect
    // on verdict.
    std::optional<base::ElapsedTimer> elapsed_;
  };

  explicit ClassifyUrlNavigationThrottle(
      content::NavigationHandle* navigation_handle);

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

  // Triggers a URL check; the result might be processed either synchronously
  // or asynchronously.
  void CheckURL();

  // The triggered callback; results will be written onto check.
  void OnURLCheckDone(ClassifyUrlCheckList::Key key,
                      const GURL& url,
                      FilteringBehavior behavior,
                      FilteringBehaviorReason reason,
                      bool uncertain);

  // Change state of the throttle and record metrics.
  std::optional<ThrottleCheckResult> NextNavigationState(
      ClassifyUrlThrottleStatus status);

  // Defers the navigation to accommodate the interstitial and shows that
  // interstitial.
  ThrottleCheckResult DeferAndScheduleInterstitial(
      ClassifyUrlCheckList::FilteringResult result);

  // Interstitial handling
  void ScheduleInterstitial(ClassifyUrlCheckList::FilteringResult result);
  void ShowInterstitial(ClassifyUrlCheckList::FilteringResult result);
  void OnInterstitialResult(
      ClassifyUrlCheckList::FilteringResult result,
      SupervisedUserNavigationThrottle::CallbackActions action,
      bool already_sent_request,
      bool is_main_frame);

  // All pending and completed checks.
  ClassifyUrlCheckList list_;

  // True iff one of navigation events returned ::DEFER.
  bool deferred_{false};

  // Timers forming a continuum of time, only recorded in unblocked navigation
  // (success) case.
  std::optional<base::ElapsedTimer> waiting_for_decision_;

  raw_ptr<supervised_user::SupervisedUserURLFilter> url_filter_;
  base::WeakPtrFactory<ClassifyUrlNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_CLASSIFY_URL_NAVIGATION_THROTTLE_H_
