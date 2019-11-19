// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"
#endif

// static
std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(
    content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  if (!profile->IsChild())
    return nullptr;

  return base::WrapUnique(new SupervisedUserGoogleAuthNavigationThrottle(
      profile, navigation_handle));
}

SupervisedUserGoogleAuthNavigationThrottle::
    SupervisedUserGoogleAuthNavigationThrottle(
        Profile* profile,
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      child_account_service_(ChildAccountServiceFactory::GetForProfile(profile))
#if defined(OS_ANDROID)
      ,
      has_shown_reauth_(false)
#endif
{
}

SupervisedUserGoogleAuthNavigationThrottle::
    ~SupervisedUserGoogleAuthNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

const char* SupervisedUserGoogleAuthNavigationThrottle::GetNameForLogging() {
  return "SupervisedUserGoogleAuthNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::WillStartOrRedirectRequest() {
  const GURL& url = navigation_handle()->GetURL();
  if (!google_util::IsGoogleSearchUrl(url) &&
      !google_util::IsGoogleHomePageUrl(url) &&
      !google_util::IsYoutubeDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS)) {
    return content::NavigationThrottle::PROCEED;
  }

  content::NavigationThrottle::ThrottleCheckResult result = ShouldProceed();

  if (result.action() == content::NavigationThrottle::DEFER) {
    google_auth_state_subscription_ =
        child_account_service_->ObserveGoogleAuthState(
            base::Bind(&SupervisedUserGoogleAuthNavigationThrottle::
                           OnGoogleAuthStateChanged,
                       base::Unretained(this)));
  }

  return result;
}

void SupervisedUserGoogleAuthNavigationThrottle::OnGoogleAuthStateChanged() {
  content::NavigationThrottle::ThrottleCheckResult result = ShouldProceed();

  switch (result.action()) {
    case content::NavigationThrottle::PROCEED: {
      google_auth_state_subscription_.reset();
      Resume();
      break;
    }
    case content::NavigationThrottle::CANCEL:
    case content::NavigationThrottle::CANCEL_AND_IGNORE: {
      CancelDeferredNavigation(result);
      break;
    }
    case content::NavigationThrottle::DEFER: {
      // Keep blocking.
      break;
    }
    case content::NavigationThrottle::BLOCK_REQUEST:
    case content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
    case content::NavigationThrottle::BLOCK_RESPONSE: {
      NOTREACHED();
    }
  }
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::ShouldProceed() {
  ChildAccountService::AuthState authStatus =
      child_account_service_->GetGoogleAuthState();
  if (authStatus == ChildAccountService::AuthState::AUTHENTICATED)
    return content::NavigationThrottle::PROCEED;
  if (authStatus == ChildAccountService::AuthState::PENDING)
    return content::NavigationThrottle::DEFER;

#if defined(OS_CHROMEOS)
  // A credentials re-mint is already underway when we reach here (Mirror
  // account reconciliation). Nothing to do here except block the navigation
  // while re-minting is underway.
  return content::NavigationThrottle::DEFER;
#elif defined(OS_ANDROID)
  if (!has_shown_reauth_) {
    has_shown_reauth_ = true;

    content::WebContents* web_contents = navigation_handle()->GetWebContents();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo();
    ReauthenticateChildAccount(
        web_contents, account_info.email,
        base::Bind(&SupervisedUserGoogleAuthNavigationThrottle::
                       OnReauthenticationResult,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  return content::NavigationThrottle::DEFER;
#else
  NOTREACHED();

  // This should never happen but needs to be included to avoid compilation
  // error on debug builds.
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
#endif
}

void SupervisedUserGoogleAuthNavigationThrottle::OnReauthenticationResult(
    bool reauth_successful) {
  if (reauth_successful) {
    // If reauthentication was not successful, wait until the cookies are
    // refreshed, which will call us back separately.
    return;
  }

  // Otherwise cancel immediately.
  CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
}
