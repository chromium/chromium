// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/features.h"

namespace first_party_sets {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

void RecordResumeOnTimeout(bool is_timeout) {
  base::UmaHistogramBoolean("FirstPartySets.NavigationThrottle.ResumeOnTimeout",
                            is_timeout);
}

}  // namespace

FirstPartySetsNavigationThrottle::FirstPartySetsNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    FirstPartySetsPolicyService& service)
    : content::NavigationThrottle(navigation_handle), service_(service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FirstPartySetsNavigationThrottle::~FirstPartySetsNavigationThrottle() = default;

ThrottleCheckResult FirstPartySetsNavigationThrottle::WillStartRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (service_->is_enabled() && !service_->is_ready()) {
    service_->RegisterThrottleResumeCallback(
        base::BindOnce(&FirstPartySetsNavigationThrottle::OnReadyToResume,
                       weak_factory_.GetWeakPtr()));
    // Setup timer
    resume_navigation_timer_.Start(
        FROM_HERE,
        net::features::kWaitForFirstPartySetsInitNavigationThrottleTimeout
            .Get(),
        base::BindOnce(&FirstPartySetsNavigationThrottle::OnTimeOut,
                       weak_factory_.GetWeakPtr()));

    CHECK(!throttle_navigation_timer_.has_value());
    throttle_navigation_timer_ = {base::ElapsedTimer()};

    return content::NavigationThrottle::DEFER;
  }
  return content::NavigationThrottle::PROCEED;
}

const char* FirstPartySetsNavigationThrottle::GetNameForLogging() {
  return "FirstPartySetsNavigationThrottle";
}

// static
std::unique_ptr<FirstPartySetsNavigationThrottle>
FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  // The `service` might be null for some irregular profiles.
  // TODO(crbug.com/40233408): regular profiles and guest sessions
  // aren't mutually exclusive on ChromeOS.
  if (!profile->IsRegularProfile() || profile->IsGuestSession())
    return nullptr;

  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile);
  CHECK(service);
  if (service->is_ready() ||
      !base::FeatureList::IsEnabled(
          net::features::kWaitForFirstPartySetsInit) ||
      net::features::kWaitForFirstPartySetsInitNavigationThrottleTimeout.Get()
          .is_zero() ||
      navigation_handle->GetParentFrameOrOuterDocument()) {
    return nullptr;
  }
  return std::make_unique<FirstPartySetsNavigationThrottle>(navigation_handle,
                                                            *service);
}

void FirstPartySetsNavigationThrottle::OnTimeOut() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!resume_navigation_timer_.IsRunning());
  RecordResumeOnTimeout(true);
  Resume();
}

void FirstPartySetsNavigationThrottle::OnReadyToResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the timer is not running, that means the timeout has occurred and the
  // navigation has been resumed by `OnTimeOut`, so we don't need to resume
  // again.
  if (!resume_navigation_timer_.IsRunning()) {
    CHECK(resumed_);
    return;
  }
  // Stop the timer to make sure we won't try to resume again due to hitting
  // the timeout.
  resume_navigation_timer_.Stop();
  RecordResumeOnTimeout(false);
  Resume();
}

void FirstPartySetsNavigationThrottle::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!resumed_);
  resumed_ = true;

  CHECK(throttle_navigation_timer_.has_value());
  base::UmaHistogramTimes("FirstPartySets.NavigationThrottle.ResumeDelta",
                          throttle_navigation_timer_->Elapsed());
  NavigationThrottle::Resume();
}

}  // namespace first_party_sets
