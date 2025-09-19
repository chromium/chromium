// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/family_link_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
bool ShouldShowReAuthInterstitial(
    content::NavigationHandle& navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle.GetWebContents()->GetBrowserContext());
  ChildAccountService* child_account_service =
      ChildAccountServiceFactory::GetForProfile(profile);
  return SupervisedUserVerificationPage::ShouldShowPage(*child_account_service);
}
#endif
}  // namespace

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::WillProcessRequest() {
  // We do not yet support prerendering for supervised users.
  if (navigation_handle()->IsInPrerenderedMainFrame()) {
    return CANCEL;
  }

  CheckURL();

  // It is possible that check was synchronous. If that's the case,
  // short-circuit and show the interstitial immediately, also breaking the
  // redirect chain.
  if (auto result = list_.GetBlockingResult(); result.has_value()) {
    // Defer navigation for the duration of interstitial.
    return DeferAndScheduleInterstitial(*result);
  }

  return PROCEED;
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
  list_.MarkNavigationRequestsCompleted();

  if (!list_.IsDecided()) {
    // Defer navigation until checks are conclusive
    waiting_for_decision_.emplace();
    deferred_ = true;
    return DEFER;
  }

  if (auto result = list_.GetBlockingResult(); result.has_value()) {
    // Defer navigation for the duration of interstitial.
    return DeferAndScheduleInterstitial(*result);
  }

  // All checks decided that it's safe to proceed.
  base::UmaHistogramTimes(kClassifiedEarlierThanContentResponseHistogramName,
                          list_.ElapsedSinceDecided());
  VLOG(1) << "Decision was ready ahead of time:" << list_.ElapsedSinceDecided();
  base::UmaHistogramEnumeration(kClassifyUrlThrottleFinalStatusHistogramName,
                                ClassifyUrlThrottleFinalStatus::kAllowed);
  return PROCEED;
}

void ClassifyUrlNavigationThrottle::CheckURL() {
  const GURL& url = currently_navigated_url();
  ClassifyUrlCheckList::Key key = list_.NewCheck();

  if (navigation_handle()->IsInPrimaryMainFrame()) {
    url_filter()->GetFilteringBehaviorWithAsyncChecks(
        url,
        base::BindOnce(&ClassifyUrlNavigationThrottle::OnURLCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), key),
        ShouldContentSkipParentAllowlistFiltering(
            navigation_handle()->GetWebContents()->GetOutermostWebContents()),
        FilteringContext::kNavigationThrottle,
        navigation_handle()->GetPageTransition());
  } else {
    url_filter()->GetFilteringBehaviorForSubFrameWithAsyncChecks(
        url, navigation_handle()->GetWebContents()->GetVisibleURL(),
        base::BindOnce(&ClassifyUrlNavigationThrottle::OnURLCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), key),
        FilteringContext::kNavigationThrottle,
        navigation_handle()->GetPageTransition());
  }
}

void ClassifyUrlNavigationThrottle::OnURLCheckDone(
    ClassifyUrlCheckList::Key key,
    SupervisedUserURLFilter::Result filtering_result) {
  if (list_.IsDecided()) {
    // If the verdict is already determined there's no point in processing the
    // check. This will reduce noise in metrics, but side-effects might apply
    // (eg. populating classification cache).
    return;
  }

  // Updates the check results. This invalidates the InPending state.
  list_.UpdateCheck(key, filtering_result);

  if (!list_.IsDecided()) {
    // Stop right here. More checks need to complete to know if navigation
    // should be deferred or interstitial presented.
    return;
  }

  // Checks are completed before needed
  if (!deferred_) {
    // If behavior == FilteringBehavior::kAllow then WillProcessResponse will
    // eventually pick up. Otherwise, if the call is synchronous, the calling
    // request or redirect event will test if the navigation should be blocked
    // immediately.
    return;
  }

  // Checks are completed after they were needed by WillProcessResponse.
  if (auto blocking_result = list_.GetBlockingResult();
      blocking_result.has_value()) {
    ScheduleInterstitial(*blocking_result);
  } else {
    base::UmaHistogramTimes(kClassifiedLaterThanContentResponseHistogramName,
                            waiting_for_decision_->Elapsed());
    VLOG(1) << "Had to delay decision:" << waiting_for_decision_->Elapsed();
    base::UmaHistogramEnumeration(kClassifyUrlThrottleFinalStatusHistogramName,
                                  ClassifyUrlThrottleFinalStatus::kAllowed);
    Resume();
  }
}

void ClassifyUrlNavigationThrottle::ScheduleInterstitial(
    SupervisedUserURLFilter::Result result) {
  // Don't show interstitial synchronously - it doesn't seem like a good idea to
  // show an interstitial right in the middle of a call into a
  // NavigationThrottle. This also lets OnInterstitialResult to be invoked
  // synchronously, once a callback is passed into the
  // SupervisedUserNavigationObserver.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClassifyUrlNavigationThrottle::ShowInterstitial,
                     weak_ptr_factory_.GetWeakPtr(), result));
}

void ClassifyUrlNavigationThrottle::ShowInterstitial(
    SupervisedUserURLFilter::Result result) {
  SupervisedUserNavigationObserver::OnRequestBlocked(
      navigation_handle()->GetWebContents(), result.url, result.reason,
      navigation_handle()->GetNavigationId(),
      navigation_handle()->GetFrameTreeNodeId(),
      base::BindRepeating(&ClassifyUrlNavigationThrottle::OnInterstitialResult,
                          weak_ptr_factory_.GetWeakPtr(), result));
}

void ClassifyUrlNavigationThrottle::OnInterstitialResult(
    SupervisedUserURLFilter::Result result,
    InterstitialResultCallbackActions action,
    bool already_sent_request,
    bool is_main_frame) {
  switch (action) {
    case InterstitialResultCallbackActions::kCancelNavigation: {
      CancelDeferredNavigation(CANCEL);
      break;
    }
    case InterstitialResultCallbackActions::kCancelWithInterstitial: {
      CHECK(navigation_handle());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      if (ShouldShowReAuthInterstitial(*navigation_handle())) {
        // Show the re-authentication interstitial if the user signed out of
        // the content area, as parent's approval requires authentication.
        // This interstitial is only available on Linux/Mac/Windows as
        // ChromeOS and Android have different re-auth mechanisms.
        CancelDeferredNavigation(
            content::NavigationThrottle::ThrottleCheckResult(
                CANCEL, net::ERR_BLOCKED_BY_CLIENT,
                CreateReauthenticationInterstitialForBlockedSites(
                    *navigation_handle(), result.reason)));
        return;
      }
#endif
      CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT,
          GetInterstitialHTML(result, already_sent_request, is_main_frame)));

      break;
    }
  }
}

std::string ClassifyUrlNavigationThrottle::GetInterstitialHTML(
    SupervisedUserURLFilter::Result result,
    bool already_sent_request,
    bool is_main_frame) const {
#if BUILDFLAG(IS_ANDROID)
  if (supervised_user_service()->IsLocalBrowserFilteringEnabled() &&
      UseInterstitialForLocalSupervision()) {
    return SupervisedUserInterstitial::GetHTMLContentsWithoutApprovals(
        result.url, g_browser_process->GetApplicationLocale());
  }
#endif
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
  return SupervisedUserInterstitial::GetHTMLContentsWithApprovals(
      supervised_user_service(), profile->GetPrefs(), result.reason,
      already_sent_request, is_main_frame,
      g_browser_process->GetApplicationLocale());
}

const GURL& ClassifyUrlNavigationThrottle::currently_navigated_url() const {
  return navigation_handle()->GetURL();
}

SupervisedUserURLFilter* ClassifyUrlNavigationThrottle::url_filter() const {
  return supervised_user_service()->GetURLFilter();
}

SupervisedUserService* ClassifyUrlNavigationThrottle::supervised_user_service()
    const {
  return SupervisedUserServiceFactory::GetForProfile(
      Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));
}

void ClassifyUrlNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  Profile* profile = Profile::FromBrowserContext(
      registry.GetNavigationHandle().GetWebContents()->GetBrowserContext());

  // Off the record profiles don't have the infrastructure to support the
  // ClassifyUrlNavigationThrottle, so we should not add it.
  if (profile->IsOffTheRecord()) {
    return;
  }

  // This check is not making logical difference as the throttle would allow
  // this navigation anyway, but in this case no metrics will be recorded.
  if (SupervisedUserServiceFactory::GetInstance()
          ->GetForProfile(profile)
          ->GetURLFilter()
          ->GetWebFilterType() == WebFilterType::kDisabled) {
    return;
  }

  registry.AddThrottle(
      base::WrapUnique(new ClassifyUrlNavigationThrottle(registry)));
}

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::DeferAndScheduleInterstitial(
    SupervisedUserURLFilter::Result result) {
  ScheduleInterstitial(result);
  deferred_ = true;
  return DEFER;
}

void ClassifyUrlNavigationThrottle::CancelDeferredNavigation(
    ThrottleCheckResult result) {
  base::UmaHistogramEnumeration(kClassifyUrlThrottleFinalStatusHistogramName,
                                ClassifyUrlThrottleFinalStatus::kBlocked);
  content::NavigationThrottle::CancelDeferredNavigation(result);
}

const char* ClassifyUrlNavigationThrottle::GetNameForLogging() {
  return "ClassifyUrlNavigationThrottle";
}

ClassifyUrlNavigationThrottle::ClassifyUrlNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}
ClassifyUrlNavigationThrottle::~ClassifyUrlNavigationThrottle() = default;

ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::ClassifyUrlCheckList() =
    default;
ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::~ClassifyUrlCheckList() =
    default;

void ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::
    MarkNavigationRequestsCompleted() {
  CHECK(!new_checks_disabled_);
  new_checks_disabled_ = true;
}

ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::Key
ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::NewCheck() {
  CHECK(!new_checks_disabled_) << "Can't add new checks after sealing";
  results_.emplace_back();
  return results_.size() - 1;
}

void ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::UpdateCheck(
    Key key,
    SupervisedUserURLFilter::Result result) {
  // Every time a check is completed update the timer, so that it only measures
  // elapsed time from the last meaningful check to when the verdict was needed.
  elapsed_.emplace();
  results_[key] = result;
}

base::TimeDelta
ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::ElapsedSinceDecided()
    const {
  return elapsed_->Elapsed();
}

std::optional<SupervisedUserURLFilter::Result>
ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::GetBlockingResult() const {
  for (const auto& result : results_) {
    if (!result.has_value()) {
      return std::nullopt;
    }
    if (result->IsBlocked()) {
      return result;
    }
  }
  return std::nullopt;
}

bool ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::IsDecided() const {
  // Overall verdict is pending when an individual check is pending,
  // or all received checks are "allowed" but more checks can be scheduled.
  for (const auto& result : results_) {
    if (!result.has_value()) {
      return false;
    }
    if (result->IsBlocked()) {
      return true;
    }
  }

  return new_checks_disabled_;
}
}  // namespace supervised_user
