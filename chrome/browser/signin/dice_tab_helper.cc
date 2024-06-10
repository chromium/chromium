// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_auth_util.h"

// static
DiceTabHelper::EnableSyncCallback
DiceTabHelper::GetEnableSyncCallbackForBrowser() {
  return base::BindRepeating([](Profile* profile,
                                signin_metrics::AccessPoint access_point,
                                signin_metrics::PromoAction promo_action,
                                content::WebContents* web_contents,
                                const CoreAccountInfo& account_info) {
    DCHECK(profile);
    Browser* browser = web_contents ? chrome::FindBrowserWithTab(web_contents)
                                    : chrome::FindBrowserWithProfile(profile);
    if (!browser) {
      return;
    }

    bool is_sync_promo = access_point ==
                         signin_metrics::AccessPoint::
                             ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN_WITH_SYNC_PROMO;
    TurnSyncOnHelper::SigninAbortedMode abort_mode =
        is_sync_promo ? TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT
                      : TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT;

    // TurnSyncOnHelper is suicidal (it will kill itself once it
    // finishes enabling sync).
    new TurnSyncOnHelper(profile, browser, access_point, promo_action,
                         account_info.account_id, abort_mode, is_sync_promo);
  });
}

// static
DiceTabHelper::ShowSigninErrorCallback
DiceTabHelper::GetShowSigninErrorCallbackForBrowser() {
  return base::BindRepeating([](Profile* profile,
                                content::WebContents* web_contents,
                                const SigninUIError& error) {
    if (!profile) {
      return;
    }
    Browser* browser = web_contents ? chrome::FindBrowserWithTab(web_contents)
                                    : chrome::FindBrowserWithProfile(profile);
    if (!browser) {
      return;
    }
    LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
        browser, error, /*from_profile_picker=*/false);
  });
}

DiceTabHelper::ResetableState::ResetableState() = default;
DiceTabHelper::ResetableState::~ResetableState() = default;
DiceTabHelper::ResetableState::ResetableState(ResetableState&& other) = default;
DiceTabHelper::ResetableState& DiceTabHelper::ResetableState::operator=(
    ResetableState&& other) = default;

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<DiceTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents) {}

DiceTabHelper::~DiceTabHelper() = default;

void DiceTabHelper::InitializeSigninFlow(
    const GURL& signin_url,
    signin_metrics::AccessPoint access_point,
    signin_metrics::Reason reason,
    signin_metrics::PromoAction promo_action,
    const GURL& redirect_url,
    bool record_signin_started_metrics,
    EnableSyncCallback enable_sync_callback,
    OnSigninHeaderReceived on_signin_header_received_callback,
    ShowSigninErrorCallback show_signin_error_callback) {
  DCHECK(signin_url.is_valid());
  DCHECK(state_.signin_url.is_empty() || state_.signin_url == signin_url);

  Reset();
  state_.redirect_url = redirect_url;
  state_.signin_url = signin_url;
  state_.signin_access_point = access_point;
  state_.signin_promo_action = promo_action;
  state_.signin_reason = reason;
  state_.enable_sync_callback = std::move(enable_sync_callback);
  state_.on_signin_header_received_callback =
      std::move(on_signin_header_received_callback);
  state_.show_signin_error_callback = std::move(show_signin_error_callback);

  is_chrome_signin_page_ = true;
  signin_page_load_recorded_ = false;

  if (reason == signin_metrics::Reason::kSigninPrimaryAccount) {
    state_.sync_signin_flow_status = SyncSigninFlowStatus::kStarted;
  }

  // This profile creation may lead to the user signing in. To speed up a
  // potential subsequent account capabililties fetch, notify IdentityManager.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  identity_manager->PrepareForAddingNewAccount();

  if (!record_signin_started_metrics) {
    return;
  }

  // Note: if a Dice signin tab is reused, `InitializeSigninFlow()` is not
  // called again, and the tab reuse does not generate new metrics.

  if (reason == signin_metrics::Reason::kSigninPrimaryAccount ||
      reason == signin_metrics::Reason::kAddSecondaryAccount) {
    // See details at go/chrome-signin-metrics-revamp.
    signin_metrics::LogSignInStarted(access_point);
  }

  if (reason == signin_metrics::Reason::kSigninPrimaryAccount) {
    signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Loading"));
  }

  if (signin_util::IsSigninPending(identity_manager)) {
    base::UmaHistogramEnumeration(
        "Signin.SigninPending.ResolutionSourceStarted", access_point,
        signin_metrics::AccessPoint::ACCESS_POINT_MAX);
  }
}

bool DiceTabHelper::IsChromeSigninPage() const {
  return is_chrome_signin_page_;
}

bool DiceTabHelper::IsSyncSigninInProgress() const {
  return state_.sync_signin_flow_status == SyncSigninFlowStatus::kStarted;
}

void DiceTabHelper::OnSyncSigninFlowComplete() {
  // The flow is complete, reset to initial state.
  Reset();
}

void DiceTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_chrome_signin_page_) {
    return;
  }

  // Ignore internal navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsSigninPageNavigation(navigation_handle)) {
    // Navigating away from the signin page.
    // Note that currently any indication of a navigation is enough to consider
    // this tab unsuitable for re-use, even if the navigation does not end up
    // committing.
    is_chrome_signin_page_ = false;
  }
}

void DiceTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_chrome_signin_page_) {
    return;
  }

  // Ignore internal navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsSigninPageNavigation(navigation_handle)) {
    // Navigating away from the signin page.
    // Note that currently any indication of a navigation is enough to consider
    // this tab unsuitable for re-use, even if the navigation does not end up
    // committing.
    is_chrome_signin_page_ = false;
    return;
  }

  if (state_.signin_reason == signin_metrics::Reason::kSigninPrimaryAccount &&
      !signin_page_load_recorded_) {
    signin_page_load_recorded_ = true;
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Shown"));
  }
}

bool DiceTabHelper::IsSigninPageNavigation(
    content::NavigationHandle* navigation_handle) const {
  return !navigation_handle->IsErrorPage() &&
         navigation_handle->GetRedirectChain()[0] == state_.signin_url &&
         gaia::HasGaiaSchemeHostPort(navigation_handle->GetURL());
}

void DiceTabHelper::Reset() {
  state_ = {};
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DiceTabHelper);
