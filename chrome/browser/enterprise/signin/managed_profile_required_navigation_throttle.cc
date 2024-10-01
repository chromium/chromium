// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"

#include <memory>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_controller_client.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace {

std::u16string GetManagerRequestingProfileSeparation(
    const DiceWebSigninInterceptor& interceptor,
    const std::u16string& profile_management_domain) {
  const std::string& email = interceptor.intercepted_account_info().email;
  std::u16string manager;
  if (signin_util::IsProfileSeparationEnforcedByPolicies(
          interceptor.intercepted_account_profile_separation_policies()
              .value_or(policy::ProfileSeparationPolicies()))) {
    manager = base::UTF8ToUTF16(enterprise_util::GetDomainFromEmail(email));
  } else if (!profile_management_domain.empty()) {
    manager = profile_management_domain;
  } else if (auto device_manager = chrome::GetDeviceManagerIdentity();
             device_manager) {
    manager = base::UTF8ToUTF16(*device_manager);
  }

  return manager;
}

}  // namespace

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
      base::UTF8ToUTF16(
          chrome::GetEnterpriseAccountDomain(*profile).value_or(std::string())),
      interceptor);
}

ManagedProfileRequiredNavigationThrottle::
    ManagedProfileRequiredNavigationThrottle(
        content::NavigationHandle* navigation_handle,
        const std::u16string& profile_management_domain,
        DiceWebSigninInterceptor* signin_interceptor)
    : content::NavigationThrottle(navigation_handle),
      profile_management_domain_(profile_management_domain),
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
      GetManagerRequestingProfileSeparation(*signin_interceptor_,
                                            profile_management_domain_),
      base::UTF8ToUTF16(signin_interceptor_->intercepted_account_info().email),
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
