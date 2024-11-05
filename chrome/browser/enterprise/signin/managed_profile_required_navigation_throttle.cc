// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_controller_client.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

// static
std::unique_ptr<ManagedProfileRequiredNavigationThrottle>
ManagedProfileRequiredNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(
          features::kEnterpriseUpdatedProfileCreationScreen)) {
    return nullptr;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsInPrerenderedMainFrame()) {
    return nullptr;
  }
  auto* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  auto* interceptor = DiceWebSigninInterceptorFactory::GetForProfile(profile);
  if (!interceptor) {
    return nullptr;
  }
  return std::make_unique<ManagedProfileRequiredNavigationThrottle>(
      navigation_handle,
      interceptor);
}

ManagedProfileRequiredNavigationThrottle::
    ManagedProfileRequiredNavigationThrottle(
        content::NavigationHandle* navigation_handle,
        DiceWebSigninInterceptor* signin_interceptor)
    : content::NavigationThrottle(navigation_handle),
      signin_interceptor_(signin_interceptor) {}

ManagedProfileRequiredNavigationThrottle::
    ~ManagedProfileRequiredNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillStartRequest() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillRedirectRequest() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillProcessResponse() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillFailRequest() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::ProcessThrottleEvent() {
  if (!signin_interceptor_->managed_profile_creation_required_by_policy() ||
      signin_interceptor_->web_contents() !=
          navigation_handle()->GetWebContents()) {
    return PROCEED;
  }

  auto managed_profile_required = std::make_unique<ManagedProfileRequiredPage>(
      navigation_handle()->GetWebContents(), navigation_handle()->GetURL(),
      std::make_unique<ManagedProfileRequiredControllerClient>(
          navigation_handle()->GetWebContents(),
          navigation_handle()->GetURL()));

  std::string error_page_content = managed_profile_required->GetHTMLContents();
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle(), std::move(managed_profile_required));
  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
}

const char* ManagedProfileRequiredNavigationThrottle::GetNameForLogging() {
  return "ManagedProfileRequiredNavigationThrottle";
}
