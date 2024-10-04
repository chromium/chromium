// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

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
#include "url/gurl.h"

namespace supervised_user {

namespace {
std::ostream& operator<<(std::ostream& stream,
                         ClassifyUrlThrottleStatus status) {
  switch (status) {
    case ClassifyUrlThrottleStatus::kContinue:
      stream << "Continue";
      return stream;
    case ClassifyUrlThrottleStatus::kProceed:
      stream << "Proceed";
      return stream;
    case ClassifyUrlThrottleStatus::kDefer:
      stream << "Defer";
      return stream;
    case ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial:
      stream << "DeferAndScheduleInterstitial";
      return stream;
    case ClassifyUrlThrottleStatus::kCancel:
      stream << "Cancel";
      return stream;
    case ClassifyUrlThrottleStatus::kResume:
      stream << "Resume";
      return stream;
    case ClassifyUrlThrottleStatus::kCancelDeferredNavigation:
      stream << "CancelDeferredNavigation";
      return stream;
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::WillProcessRequest() {
  // We do not yet support prerendering for supervised users.
  if (navigation_handle()->IsInPrerenderedMainFrame()) {
    return *NextNavigationState(ClassifyUrlThrottleStatus::kCancel);
  }
  CheckURL();

  // It is possible that check was synchronous. If that's the case,
  // short-circuit and show the interstitial immediately, also breaking the
  // redirect chain.
  if (auto result = list_.GetBlockingResult(); result.has_value()) {
    // Defer navigation for the duration of interstitial.
    return DeferAndScheduleInterstitial(*result);
  }

  return *NextNavigationState(ClassifyUrlThrottleStatus::kContinue);
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
    return *NextNavigationState(ClassifyUrlThrottleStatus::kDefer);
  }

  if (auto result = list_.GetBlockingResult(); result.has_value()) {
    // Defer navigation for the duration of interstitial.
    return DeferAndScheduleInterstitial(*result);
  }

  // All checks decided that it's safe to proceed.
  base::UmaHistogramTimes(kClassifiedEarlierThanContentResponseHistogramName,
                          list_.ElapsedSinceDecided());
  VLOG(1) << "Decision was ready ahead of time:" << list_.ElapsedSinceDecided();
  return *NextNavigationState(ClassifyUrlThrottleStatus::kProceed);
}

void ClassifyUrlNavigationThrottle::CheckURL() {
  const GURL& url = currently_navigated_url();
  ClassifyUrlCheckList::Key key = list_.NewCheck();

  if (navigation_handle()->IsInPrimaryMainFrame()) {
    url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
        url,
        base::BindOnce(&ClassifyUrlNavigationThrottle::OnURLCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), key, url),
        supervised_user::ShouldContentSkipParentAllowlistFiltering(
            navigation_handle()->GetWebContents()->GetOutermostWebContents()));
  } else {
    url_filter_->GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
        url, navigation_handle()->GetWebContents()->GetVisibleURL(),
        base::BindOnce(&ClassifyUrlNavigationThrottle::OnURLCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), key, url));
  }
}

void ClassifyUrlNavigationThrottle::OnURLCheckDone(
    ClassifyUrlCheckList::Key key,
    const GURL& url,
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool uncertain) {
  if (list_.IsDecided()) {
    // If the verdict is already determined there's no point in processing the
    // check. This will reduce noise in metrics, but side-effects might apply
    // (eg. populating classification cache).
    return;
  }

  // Updates the check results. This invalidates the InPending state.
  list_.UpdateCheck(key, {url, behavior, reason});

  SupervisedUserURLFilter::RecordFilterResultEvent(
      behavior, reason, /*is_filtering_behavior_known=*/!uncertain,
      navigation_handle()->GetPageTransition());

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
  if (auto result = list_.GetBlockingResult(); result.has_value()) {
    ScheduleInterstitial(*result);
  } else {
    base::UmaHistogramTimes(kClassifiedLaterThanContentResponseHistogramName,
                            waiting_for_decision_->Elapsed());
    VLOG(1) << "Had to delay decision:" << waiting_for_decision_->Elapsed();
    NextNavigationState(ClassifyUrlThrottleStatus::kResume);
  }
}

void ClassifyUrlNavigationThrottle::ScheduleInterstitial(
    ClassifyUrlCheckList::FilteringResult result) {
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
    ClassifyUrlCheckList::FilteringResult result) {
  SupervisedUserNavigationObserver::OnRequestBlocked(
      navigation_handle()->GetWebContents(), result.url, result.reason,
      navigation_handle()->GetNavigationId(),
      navigation_handle()->GetFrameTreeNodeId(),
      base::BindRepeating(&ClassifyUrlNavigationThrottle::OnInterstitialResult,
                          weak_ptr_factory_.GetWeakPtr(), result));
}

void ClassifyUrlNavigationThrottle::OnInterstitialResult(
    ClassifyUrlCheckList::FilteringResult result,
    SupervisedUserNavigationThrottle::CallbackActions action,
    bool already_sent_request,
    bool is_main_frame) {
  switch (action) {
    case SupervisedUserNavigationThrottle::kCancelNavigation: {
      CancelDeferredNavigation(CANCEL);
      break;
    }
    case SupervisedUserNavigationThrottle::kCancelWithInterstitial: {
      CHECK(navigation_handle());
// LINT.IfChange(cancel_with_interstitial)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      if (ShouldShowReAuthInterstitial(*navigation_handle(), is_main_frame)) {
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
      // LINT.ThenChange(//chrome/browser/supervised_user/supervised_user_navigation_throttle.cc:cancel_with_interstitial)
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
      std::string interstitial_html =
          SupervisedUserInterstitial::GetHTMLContents(
              SupervisedUserServiceFactory::GetForProfile(profile),
              profile->GetPrefs(), result.reason, already_sent_request,
              is_main_frame, g_browser_process->GetApplicationLocale());
      CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html));
      break;
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
  if (!profile->IsChild()) {
    return nullptr;
  }
  return ClassifyUrlNavigationThrottle::MakeUnique(navigation_handle);
}

std::optional<ClassifyUrlNavigationThrottle::ThrottleCheckResult>
ClassifyUrlNavigationThrottle::NextNavigationState(
    ClassifyUrlThrottleStatus status) {
  VLOG(1) << status;
  base::UmaHistogramEnumeration(kClassifyUrlThrottleStatusHistogramName,
                                status);

  switch (status) {
    case ClassifyUrlThrottleStatus::kContinue:
    case ClassifyUrlThrottleStatus::kProceed:
      return NavigationThrottle::PROCEED;
    case ClassifyUrlThrottleStatus::kDefer:
    case ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial:
      deferred_ = true;
      return NavigationThrottle::DEFER;
    case ClassifyUrlThrottleStatus::kCancel:
      return NavigationThrottle::CANCEL;
    case ClassifyUrlThrottleStatus::kResume:
      Resume();
      return std::nullopt;
    case ClassifyUrlThrottleStatus::kCancelDeferredNavigation:
      return std::nullopt;
  }
}
ClassifyUrlNavigationThrottle::ThrottleCheckResult
ClassifyUrlNavigationThrottle::DeferAndScheduleInterstitial(
    ClassifyUrlCheckList::FilteringResult result) {
  ScheduleInterstitial(result);
  return *NextNavigationState(
      ClassifyUrlThrottleStatus::kDeferAndScheduleInterstitial);
}

void ClassifyUrlNavigationThrottle::CancelDeferredNavigation(
    ThrottleCheckResult result) {
  content::NavigationThrottle::CancelDeferredNavigation(result);
  NextNavigationState(ClassifyUrlThrottleStatus::kCancelDeferredNavigation);
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
    FilteringResult result) {
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

std::optional<
    ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::FilteringResult>
ClassifyUrlNavigationThrottle::ClassifyUrlCheckList::GetBlockingResult() const {
  for (const auto& result : results_) {
    if (!result.has_value()) {
      return std::nullopt;
    }
    if (result->behavior == FilteringBehavior::kBlock) {
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
    if (result->behavior == FilteringBehavior::kBlock) {
      return true;
    }
  }

  return new_checks_disabled_;
}
}  // namespace supervised_user
