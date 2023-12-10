// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// static
std::unique_ptr<SupervisedUserNavigationThrottle>
SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  CHECK(profile);
  if (!supervised_user::IsUrlFilteringEnabled(*profile->GetPrefs())) {
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
      behavior_(supervised_user::FilteringBehavior::kInvalid) {}

SupervisedUserNavigationThrottle::~SupervisedUserNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserNavigationThrottle::CheckURL() {
  deferred_ = false;
  DCHECK_EQ(supervised_user::FilteringBehavior::kInvalid, behavior_);

  // We do not yet support prerendering for supervised users.
  if (navigation_handle()->IsInPrerenderedMainFrame()) {
    return NavigationThrottle::CANCEL;
  }

  GURL url = navigation_handle()->GetURL();

  bool skip_manual_parent_filter =
      supervised_user::ShouldContentSkipParentAllowlistFiltering(
          navigation_handle()->GetWebContents()->GetOutermostWebContents());

  bool got_result = false;

  if (navigation_handle()->IsInPrimaryMainFrame()) {
    got_result = url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
        url,
        base::BindOnce(&SupervisedUserNavigationThrottle::OnCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), url),
        skip_manual_parent_filter);
  } else {
    got_result = url_filter_->GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
        url, navigation_handle()->GetWebContents()->GetVisibleURL(),
        base::BindOnce(&SupervisedUserNavigationThrottle::OnCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }

  DCHECK_EQ(got_result,
            behavior_ != supervised_user::FilteringBehavior::kInvalid);
  // If we got a "not blocked" result synchronously, don't defer.
  deferred_ =
      !got_result || (behavior_ == supervised_user::FilteringBehavior::kBlock);
  if (got_result) {
    behavior_ = supervised_user::FilteringBehavior::kInvalid;
  }
  if (deferred_) {
    return NavigationThrottle::DEFER;
  }
  return NavigationThrottle::PROCEED;
}

void SupervisedUserNavigationThrottle::ShowInterstitial(
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason) {
  // Don't show interstitial synchronously - it doesn't seem like a good idea to
  // show an interstitial right in the middle of a call into a
  // NavigationThrottle. This also lets OnInterstitialResult to be invoked
  // synchronously, once a callback is passed into the
  // SupervisedUserNavigationObserver.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SupervisedUserNavigationThrottle::ShowInterstitialAsync,
                     weak_ptr_factory_.GetWeakPtr(), reason));
}

void SupervisedUserNavigationThrottle::ShowInterstitialAsync(
    supervised_user::FilteringBehaviorReason reason) {
  // May not yet have been set when ShowInterstitial was called, but should have
  // been set by the time this is invoked.
  DCHECK(deferred_);
  SupervisedUserNavigationObserver::OnRequestBlocked(
      navigation_handle()->GetWebContents(), navigation_handle()->GetURL(),
      reason, navigation_handle()->GetNavigationId(),
      navigation_handle()->GetFrameTreeNodeId(),
      base::BindRepeating(
          &SupervisedUserNavigationThrottle::OnInterstitialResult,
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
    supervised_user::FilteringBehavior behavior,
    supervised_user::FilteringBehaviorReason reason,
    bool uncertain) {
  DCHECK_EQ(supervised_user::FilteringBehavior::kInvalid, behavior_);

  // If we got a result synchronously, pass it back to ShowInterstitialIfNeeded.
  if (!deferred_) {
    behavior_ = behavior;
  }

  reason_ = reason;

  ui::PageTransition transition = navigation_handle()->GetPageTransition();

  supervised_user::SupervisedUserURLFilter::RecordFilterResultEvent(
      behavior, reason, /*is_filtering_behavior_known=*/!uncertain, transition);

  if (navigation_handle()->IsInPrimaryMainFrame()) {
    // Update navigation observer about the navigation state of the main frame.
    auto* navigation_observer =
        SupervisedUserNavigationObserver::FromWebContents(
            navigation_handle()->GetWebContents());
    if (navigation_observer) {
      navigation_observer->UpdateMainFrameFilteringStatus(behavior, reason);
    }
  }

  if (behavior == supervised_user::FilteringBehavior::kBlock) {
    ShowInterstitial(url, reason);
  } else if (deferred_) {
    Resume();
  }
}

void SupervisedUserNavigationThrottle::OnInterstitialResult(
    CallbackActions action,
    bool already_sent_request,
    bool is_main_frame) {
  switch (action) {
    case kCancelNavigation: {
      CancelDeferredNavigation(CANCEL);
      break;
    }
    case kCancelWithInterstitial: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
      std::string interstitial_html =
          supervised_user::SupervisedUserInterstitial::GetHTMLContents(
              SupervisedUserServiceFactory::GetForProfile(profile),
              profile->GetPrefs(), reason_, already_sent_request, is_main_frame,
              g_browser_process->GetApplicationLocale());
      CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html));
    }
  }
}
