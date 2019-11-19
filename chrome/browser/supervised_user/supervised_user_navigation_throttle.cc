// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// These values corresponds to SupervisedUserSafetyFilterResult in
// tools/metrics/histograms/histograms.xml. If you change anything here, make
// sure to also update histograms.xml accordingly.
enum {
  FILTERING_BEHAVIOR_ALLOW = 1,
  FILTERING_BEHAVIOR_ALLOW_UNCERTAIN,
  FILTERING_BEHAVIOR_BLOCK_BLACKLIST,
  FILTERING_BEHAVIOR_BLOCK_SAFESITES,
  FILTERING_BEHAVIOR_BLOCK_MANUAL,
  FILTERING_BEHAVIOR_BLOCK_DEFAULT,
  FILTERING_BEHAVIOR_ALLOW_WHITELIST,
  FILTERING_BEHAVIOR_MAX = FILTERING_BEHAVIOR_ALLOW_WHITELIST
};
const int kHistogramFilteringBehaviorSpacing = 100;
const int kHistogramPageTransitionMaxKnownValue =
    static_cast<int>(ui::PAGE_TRANSITION_KEYWORD_GENERATED);
const int kHistogramPageTransitionFallbackValue =
    kHistogramFilteringBehaviorSpacing - 1;
const int kHistogramMax = 800;

static_assert(kHistogramPageTransitionMaxKnownValue <
                  kHistogramPageTransitionFallbackValue,
              "HistogramPageTransition MaxKnownValue must be < FallbackValue");
static_assert(FILTERING_BEHAVIOR_MAX * kHistogramFilteringBehaviorSpacing +
                      kHistogramPageTransitionFallbackValue <
                  kHistogramMax,
              "Invalid HistogramMax value");

int GetHistogramValueForFilteringBehavior(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  switch (behavior) {
    case SupervisedUserURLFilter::ALLOW:
    case SupervisedUserURLFilter::WARN:
      if (reason == supervised_user_error_page::WHITELIST)
        return FILTERING_BEHAVIOR_ALLOW_WHITELIST;
      return uncertain ? FILTERING_BEHAVIOR_ALLOW_UNCERTAIN
                       : FILTERING_BEHAVIOR_ALLOW;
    case SupervisedUserURLFilter::BLOCK:
      switch (reason) {
        case supervised_user_error_page::BLACKLIST:
          return FILTERING_BEHAVIOR_BLOCK_BLACKLIST;
        case supervised_user_error_page::ASYNC_CHECKER:
          return FILTERING_BEHAVIOR_BLOCK_SAFESITES;
        case supervised_user_error_page::WHITELIST:
          NOTREACHED();
          break;
        case supervised_user_error_page::MANUAL:
          return FILTERING_BEHAVIOR_BLOCK_MANUAL;
        case supervised_user_error_page::DEFAULT:
          return FILTERING_BEHAVIOR_BLOCK_DEFAULT;
        case supervised_user_error_page::NOT_SIGNED_IN:
          // Should never happen, only used for requests from Webview
          NOTREACHED();
      }
      FALLTHROUGH;
    case SupervisedUserURLFilter::INVALID:
      NOTREACHED();
  }
  return 0;
}

int GetHistogramValueForTransitionType(ui::PageTransition transition_type) {
  int value =
      static_cast<int>(ui::PageTransitionStripQualifier(transition_type));
  if (0 <= value && value <= kHistogramPageTransitionMaxKnownValue)
    return value;
  NOTREACHED();
  return kHistogramPageTransitionFallbackValue;
}

void RecordFilterResultEvent(
    bool safesites_histogram,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain,
    ui::PageTransition transition_type) {
  int value =
      GetHistogramValueForFilteringBehavior(behavior, reason, uncertain) *
          kHistogramFilteringBehaviorSpacing +
      GetHistogramValueForTransitionType(transition_type);
  DCHECK_LT(value, kHistogramMax);
  // Note: We can't pass in the histogram name as a parameter to this function
  // because of how the macro works (look up the histogram on the first
  // invocation and cache it in a static variable).
  if (safesites_histogram)
    base::UmaHistogramSparse("ManagedUsers.SafetyFilter", value);
  else
    base::UmaHistogramSparse("ManagedUsers.FilteringResult", value);
}

bool IsMainFrameWhitelisted(content::WebContents* web_contents) {
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);
  if (!navigation_observer)
    return false;
  auto behavior = navigation_observer->main_frame_filtering_behavior();
  auto reason = navigation_observer->main_frame_filtering_behavior_reason();
  bool is_allowed =
      behavior == SupervisedUserURLFilter::FilteringBehavior::ALLOW;
  bool is_whitelisted =
      reason == supervised_user_error_page::FilteringBehaviorReason::WHITELIST;
  bool is_manual =
      reason == supervised_user_error_page::FilteringBehaviorReason::MANUAL;

  return is_allowed && (is_whitelisted || is_manual);
}

}  // namespace

// static
std::unique_ptr<SupervisedUserNavigationThrottle>
SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  if (!profile->IsSupervised())
    return nullptr;

  if (!navigation_handle->IsInMainFrame()) {
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    if (!service->IsSupervisedUserIframeFilterEnabled())
      return nullptr;

    // If the url in the main main frame has already been whitelisted by
    // parents, then don't create the throttle for the subframe.
    if (IsMainFrameWhitelisted(navigation_handle->GetWebContents()))
      return nullptr;
  }

  // Can't use std::make_unique because the constructor is private.
  return base::WrapUnique(
      new SupervisedUserNavigationThrottle(navigation_handle));
}

SupervisedUserNavigationThrottle::SupervisedUserNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle),
      url_filter_(
          SupervisedUserServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  navigation_handle->GetWebContents()->GetBrowserContext()))
              ->GetURLFilter()),
      deferred_(false),
      behavior_(SupervisedUserURLFilter::INVALID) {}

SupervisedUserNavigationThrottle::~SupervisedUserNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserNavigationThrottle::CheckURL() {
  deferred_ = false;
  DCHECK_EQ(SupervisedUserURLFilter::INVALID, behavior_);
  GURL url = navigation_handle()->GetURL();
  bool got_result = url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      url, base::Bind(&SupervisedUserNavigationThrottle::OnCheckDone,
                      weak_ptr_factory_.GetWeakPtr(), url));
  DCHECK_EQ(got_result, behavior_ != SupervisedUserURLFilter::INVALID);
  // If we got a "not blocked" result synchronously, don't defer.
  deferred_ = !got_result || (behavior_ == SupervisedUserURLFilter::BLOCK);
  if (got_result)
    behavior_ = SupervisedUserURLFilter::INVALID;
  if (deferred_)
    return NavigationThrottle::DEFER;
  return NavigationThrottle::PROCEED;
}

void SupervisedUserNavigationThrottle::ShowInterstitial(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason) {
  // Don't show interstitial synchronously - it doesn't seem like a good idea to
  // show an interstitial right in the middle of a call into a
  // NavigationThrottle. This also lets OnInterstitialResult to be invoked
  // synchronously, once a callback is passed into the
  // SupervisedUserNavigationObserver.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SupervisedUserNavigationThrottle::ShowInterstitialAsync,
                     weak_ptr_factory_.GetWeakPtr(), reason));
}

void SupervisedUserNavigationThrottle::ShowInterstitialAsync(
    supervised_user_error_page::FilteringBehaviorReason reason) {
  // May not yet have been set when ShowInterstitial was called, but should have
  // been set by the time this is invoked.
  DCHECK(deferred_);
  SupervisedUserNavigationObserver::OnRequestBlocked(
      navigation_handle()->GetWebContents(), navigation_handle()->GetURL(),
      reason, navigation_handle()->GetNavigationId(),
      navigation_handle()->GetFrameTreeNodeId(),
      base::Bind(&SupervisedUserNavigationThrottle::OnInterstitialResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserNavigationThrottle::WillStartRequest() {
  return CheckURL();
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserNavigationThrottle::WillRedirectRequest() {
  return CheckURL();
}

const char* SupervisedUserNavigationThrottle::GetNameForLogging() {
  return "SupervisedUserNavigationThrottle";
}

void SupervisedUserNavigationThrottle::OnCheckDone(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  DCHECK_EQ(SupervisedUserURLFilter::INVALID, behavior_);
  // If we got a result synchronously, pass it back to ShowInterstitialIfNeeded.
  if (!deferred_)
    behavior_ = behavior;

  reason_ = reason;

  ui::PageTransition transition = navigation_handle()->GetPageTransition();

  RecordFilterResultEvent(false, behavior, reason, uncertain, transition);

  // If both the static blacklist and the async checker are enabled, also record
  // SafeSites-only UMA events.
  if (url_filter_->HasBlacklist() && url_filter_->HasAsyncURLChecker() &&
      (reason == supervised_user_error_page::ASYNC_CHECKER ||
       reason == supervised_user_error_page::BLACKLIST)) {
    RecordFilterResultEvent(true, behavior, reason, uncertain, transition);
  }

  if (navigation_handle()->IsInMainFrame()) {
    // Update navigation observer about the navigation state of the main frame.
    auto* navigation_observer =
        SupervisedUserNavigationObserver::FromWebContents(
            navigation_handle()->GetWebContents());
    if (navigation_observer)
      navigation_observer->UpdateMainFrameFilteringStatus(behavior, reason);
  }

  if (behavior == SupervisedUserURLFilter::BLOCK)
    ShowInterstitial(url, reason);
  else if (deferred_)
    Resume();
}

void SupervisedUserNavigationThrottle::OnInterstitialResult(
    CallbackActions action) {
  switch (action) {
    case kCancelNavigation: {
      CancelDeferredNavigation(CANCEL);
      break;
    }
    case kCancelWithInterstitial: {
      std::string interstitial_html =
          SupervisedUserInterstitial::GetHTMLContents(
              Profile::FromBrowserContext(
                  navigation_handle()->GetWebContents()->GetBrowserContext()),
              reason_);
      CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html));
    }
  }
}
