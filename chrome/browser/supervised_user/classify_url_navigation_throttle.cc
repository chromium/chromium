// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include <list>
#include <memory>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "supervised_user_navigation_throttle.h"
#include "url/gurl.h"

namespace supervised_user {

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::WillProcessRequest() {
  // We do not yet support prerendering for supervised users.
  if (navigation_handle()->IsInPrerenderedMainFrame()) {
    return NavigationThrottle::CANCEL;
  }
  CheckURL();

  // It is possible that check was synchronous. If that's the case,
  // short-circuit and show the interstitial immediately, also breaking the
  // redirect chain.
  if (auto blocking_check = FirstBlockingCheck();
      blocking_check != checks_.end()) {
    // Defer navigation for the duration of interstitial.
    ScheduleInterstitial(*blocking_check);
    deferred_ = true;
    return NavigationThrottle::DEFER;
  }

  return NavigationThrottle::PROCEED;
}

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::WillStartRequest() {
  return WillProcessRequest();
}

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::WillRedirectRequest() {
  return WillProcessRequest();
}

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::WillProcessResponse() {
  if (!IsDecided()) {
    // Defer navigation until checks are conclusive
    deferred_ = true;
    waiting_for_decision_.emplace();
    return NavigationThrottle::DEFER;
  }

  if (auto blocking_check = FirstBlockingCheck();
      blocking_check != checks_.end()) {
    // Defer navigation for the duration of interstitial.
    ScheduleInterstitial(*blocking_check);
    deferred_ = true;
    return NavigationThrottle::DEFER;
  }

  // All checks decided that it's safe to proceed.
  base::UmaHistogramTimes(kClassifiedEarlierThanContentResponseHistogramName,
                          waiting_for_process_response_->Elapsed());
  VLOG(1) << "Decision was ready ahead of time:"
          << waiting_for_process_response_->Elapsed();
  return NavigationThrottle::PROCEED;
}

bool ClassifyUrlNavigationThrottle::IsDecided() const {
  for (const Check& check : checks_) {
    if (!check.result.has_value()) {
      return false;
    }
    if (check.result->behavior == FilteringBehavior::kBlock) {
      return true;
    }
  }
  return true;
}

std::list<ClassifyUrlNavigationThrottle::Check>::const_iterator
ClassifyUrlNavigationThrottle::FirstBlockingCheck() const {
  for (auto it = checks_.begin(); it != checks_.end(); ++it) {
    if (!it->result.has_value()) {
      return checks_.end();
    }
    if (it->result->behavior == FilteringBehavior::kBlock) {
      return it;
    }
  }
  return checks_.end();
}

void ClassifyUrlNavigationThrottle::SetCheck(const GURL& url,
                                             const CheckResult& result) {
  for (auto& check : checks_) {
    if (check.url == url && !check.result.has_value()) {
      check.result = result;
      return;
    }
  }
  NOTREACHED() << "Mismatched verdict for url: " << url;
}

void ClassifyUrlNavigationThrottle::CheckURL() {
  const GURL& url = currently_navigated_url();
  checks_.push_back({url});

  if (navigation_handle()->IsInPrimaryMainFrame()) {
    url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
        url,
        base::BindOnce(&ClassifyUrlNavigationThrottle::OnURLCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), url),
        supervised_user::ShouldContentSkipParentAllowlistFiltering(
            navigation_handle()->GetWebContents()->GetOutermostWebContents()));
  } else {
    url_filter_->GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
        url, navigation_handle()->GetWebContents()->GetVisibleURL(),
        base::BindOnce(&ClassifyUrlNavigationThrottle::OnURLCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void ClassifyUrlNavigationThrottle::OnURLCheckDone(
    const GURL& url,
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool uncertain) {
  CheckResult result{behavior, reason};
  SetCheck(url, result);

  SupervisedUserURLFilter::RecordFilterResultEvent(
      behavior, reason, /*is_filtering_behavior_known=*/!uncertain,
      navigation_handle()->GetPageTransition());

  if (!IsDecided()) {
    // Stop right here. More checks need to complete to know if navigation
    // should be deferred or interstitial presented.
    return;
  }

  // Checks are completed before needed
  if (!deferred_) {
    waiting_for_process_response_.emplace();

    // If behavior == FilteringBehavior::kAllow then WillProcessResponse will
    // eventually pick up. Otherwise, if the call is synchronous, the calling
    // request or redirect event will test if the navigation should be blocked
    // immediately.
    return;
  }

  // Checks are completed after they were needed by WillProcessResponse.
  if (behavior == FilteringBehavior::kBlock) {
    ScheduleInterstitial({url, result});
  } else {
    base::UmaHistogramTimes(kClassifiedLaterThanContentResponseHistogramName,
                            waiting_for_decision_->Elapsed());
    VLOG(1) << "Had to delay decision:" << waiting_for_decision_->Elapsed();
    Resume();
  }
}

void ClassifyUrlNavigationThrottle::ScheduleInterstitial(Check check) {
  // Don't show interstitial synchronously - it doesn't seem like a good idea to
  // show an interstitial right in the middle of a call into a
  // NavigationThrottle. This also lets OnInterstitialResult to be invoked
  // synchronously, once a callback is passed into the
  // SupervisedUserNavigationObserver.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClassifyUrlNavigationThrottle::ShowInterstitial,
                     weak_ptr_factory_.GetWeakPtr(), check));
}

void ClassifyUrlNavigationThrottle::ShowInterstitial(Check check) {
  CHECK(check.result.has_value()) << "Interstitials are only presented in the "
                                     "context of blocked navigations.";
  SupervisedUserNavigationObserver::OnRequestBlocked(
      navigation_handle()->GetWebContents(), check.url, check.result->reason,
      navigation_handle()->GetNavigationId(),
      navigation_handle()->GetFrameTreeNodeId(),
      base::BindRepeating(&ClassifyUrlNavigationThrottle::OnInterstitialResult,
                          weak_ptr_factory_.GetWeakPtr(), check));
}

void ClassifyUrlNavigationThrottle::OnInterstitialResult(
    Check check,
    SupervisedUserNavigationThrottle::CallbackActions action,
    bool already_sent_request,
    bool is_main_frame) {
  switch (action) {
    case SupervisedUserNavigationThrottle::kCancelNavigation: {
      CancelDeferredNavigation(CANCEL);
      break;
    }
    case SupervisedUserNavigationThrottle::kCancelWithInterstitial: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
      std::string interstitial_html =
          SupervisedUserInterstitial::GetHTMLContents(
              SupervisedUserServiceFactory::GetForProfile(profile),
              profile->GetPrefs(), check.result->reason, already_sent_request,
              is_main_frame, g_browser_process->GetApplicationLocale());
      CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html));
    }
  }
}

const GURL& ClassifyUrlNavigationThrottle::currently_navigated_url() const {
  return navigation_handle()->GetURL();
}

std::unique_ptr<content::NavigationThrottle>
MaybeCreateClassifyUrlNavigationThrottleFor(
    content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  CHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager == nullptr) {
    return nullptr;
  }
  if (IsPrimaryAccountSubjectToParentalControls(identity_manager) !=
      signin::Tribool::kTrue) {
    return nullptr;
  }
  return ClassifyUrlNavigationThrottle::MakeUnique(navigation_handle);
}

std::unique_ptr<ClassifyUrlNavigationThrottle>
ClassifyUrlNavigationThrottle::MakeUnique(
    content::NavigationHandle* navigation_handle) {
  return base::WrapUnique(new ClassifyUrlNavigationThrottle(navigation_handle));
}

const char* ClassifyUrlNavigationThrottle::GetNameForLogging() {
  return "ClassifyUrlNavigationThrottle";
}

ClassifyUrlNavigationThrottle::ClassifyUrlNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      url_filter_(
          SupervisedUserServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  navigation_handle->GetWebContents()->GetBrowserContext()))
              ->GetURLFilter()) {}
ClassifyUrlNavigationThrottle::~ClassifyUrlNavigationThrottle() = default;

}  // namespace supervised_user
