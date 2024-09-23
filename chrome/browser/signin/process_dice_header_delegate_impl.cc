// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_delegate_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace {

// Helper function similar to DiceTabHelper::FromWebContents(), but also handles
// the case where |contents| is nullptr.
DiceTabHelper* GetDiceTabHelperFromWebContents(content::WebContents* contents) {
  if (!contents) {
    return nullptr;
  }
  return DiceTabHelper::FromWebContents(contents);
}

// Should Sign in to Chrome for all access points when Uno is enabled. Except
// for Web Signin where we first check the user choice first on whether to
// automatically sign in or not.
void AttemptChromeSignin(CoreAccountId account_id,
                         Profile& profile,
                         signin_metrics::AccessPoint access_point) {
  CHECK(!account_id.empty());

  // For the non-ExplicitBrowserSignin equivalent counterpart, the code takes
  // care of in `SigninManager::UpdateUnconsentedPrimaryAccount()`.
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    return;
  }

  // Do not sign in if the access point is unknown.
  if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);
  if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      AccountInfo account_info =
          identity_manager->FindExtendedAccountInfoByAccountId(account_id);
      // If the user did not choose the signin choice, do not proceed with a
      // sign in from a Web Signin.
      if (SigninPrefs(*profile.GetPrefs())
              .GetChromeSigninInterceptionUserChoice(account_info.gaia) !=
          ChromeSigninUserChoice::kSignin) {
        return;
      }

      // Proceed with the access point as the choice remembered.
      access_point =
          signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_CHOICE_REMEMBERED;
    }
  }

  // This access point should only be used as a result of a non Uno flow.
  CHECK_NE(signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER,
           access_point);

  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    base::UmaHistogramEnumeration(
        "Signin.SigninManager.SigninAccessPoint", access_point,
        signin_metrics::AccessPoint::ACCESS_POINT_MAX);
    identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
        account_id, signin::ConsentLevel::kSignin, access_point);
  }
}

}  // namespace

// static
std::unique_ptr<ProcessDiceHeaderDelegateImpl>
ProcessDiceHeaderDelegateImpl::Create(content::WebContents* web_contents) {
  bool is_sync_signin_tab = false;
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
  signin_metrics::PromoAction promo_action =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  GURL redirect_url;
  EnableSyncCallback enable_sync_callback;
  OnSigninHeaderReceived on_signin_header_received;
  ShowSigninErrorCallback show_signin_error_callback;

  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
  if (tab_helper) {
    is_sync_signin_tab = tab_helper->IsSyncSigninInProgress();
    redirect_url = tab_helper->redirect_url();
    access_point = tab_helper->signin_access_point();
    promo_action = tab_helper->signin_promo_action();
    // `show_signin_error_callback` may be null if the `DiceTabHelper` was reset
    // after completion of a signin flow.
    show_signin_error_callback =
        std::move(tab_helper->GetShowSigninErrorCallback());
    if (is_sync_signin_tab) {
      enable_sync_callback = tab_helper->GetEnableSyncCallback();
    }

    on_signin_header_received = tab_helper->GetOnSigninHeaderReceived();

  } else {
    access_point = signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN;
  }

  // If there is no active `DiceTabHelper`, default to the in-browser error
  // callback. This callback does nothing if there is no browser open.
  if (!show_signin_error_callback) {
    show_signin_error_callback =
        DiceTabHelper::GetShowSigninErrorCallbackForBrowser();
  }

  return std::make_unique<ProcessDiceHeaderDelegateImpl>(
      web_contents, is_sync_signin_tab, access_point, promo_action,
      std::move(redirect_url), std::move(enable_sync_callback),
      std::move(on_signin_header_received),
      std::move(show_signin_error_callback));
}

ProcessDiceHeaderDelegateImpl::ProcessDiceHeaderDelegateImpl(
    content::WebContents* web_contents,
    bool is_sync_signin_tab,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    GURL redirect_url,
    EnableSyncCallback enable_sync_callback,
    OnSigninHeaderReceived on_signin_header_received,
    ShowSigninErrorCallback show_signin_error_callback)
    : web_contents_(web_contents->GetWeakPtr()),
      profile_(raw_ref<Profile>::from_ptr(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      is_sync_signin_tab_(is_sync_signin_tab),
      access_point_(access_point),
      promo_action_(promo_action),
      redirect_url_(std::move(redirect_url)),
      enable_sync_callback_(std::move(enable_sync_callback)),
      on_signin_header_received_(std::move(on_signin_header_received)),
      show_signin_error_callback_(std::move(show_signin_error_callback)) {
  DCHECK_EQ(!is_sync_signin_tab_, enable_sync_callback_.is_null());
  DCHECK(show_signin_error_callback_);
}

ProcessDiceHeaderDelegateImpl::~ProcessDiceHeaderDelegateImpl() = default;

bool ProcessDiceHeaderDelegateImpl::ShouldEnableSync() {
  if (IdentityManagerFactory::GetForProfile(&profile_.get())
          ->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    VLOG(1) << "Do not start sync after web sign-in [already authenticated].";
    return false;
  }

  if (!is_sync_signin_tab_) {
    VLOG(1)
        << "Do not start sync after web sign-in [not a Chrome sign-in tab].";
    return false;
  }

  if (!enable_sync_callback_) {
    VLOG(1)
        << "Do not start sync after web sign-in [no sync flow in progress].";
    return false;
  }

  return true;
}

void ProcessDiceHeaderDelegateImpl::HandleTokenExchangeSuccess(
    CoreAccountId account_id,
    bool is_new_account) {
  AttemptChromeSignin(account_id, profile_.get(), access_point_);

  // is_sync_signin_tab_ tells whether the current signin is happening in a tab
  // that was opened from a "Enable Sync" Chrome UI. Usually this is indeed a
  // sync signin, but it is not always the case: the user may abandon the sync
  // signin and do a simple web signin in the same tab instead.
  DiceWebSigninInterceptorFactory::GetForProfile(&profile_.get())
      ->MaybeInterceptWebSignin(web_contents_.get(), account_id, access_point_,
                                is_new_account, is_sync_signin_tab_);
}

void ProcessDiceHeaderDelegateImpl::EnableSync(
    const CoreAccountInfo& account_info) {
  content::WebContents* web_contents = web_contents_.get();
  DiceTabHelper* tab_helper = GetDiceTabHelperFromWebContents(web_contents);
  if (tab_helper) {
    tab_helper->OnSyncSigninFlowComplete();
  }

  if (!ShouldEnableSync()) {
    // No special treatment is needed if the user is not enabling sync.
    return;
  }

  VLOG(1) << "Start sync after web sign-in.";
  std::move(enable_sync_callback_)
      .Run(&profile_.get(), access_point_, promo_action_, web_contents,
           account_info);

  Redirect();
}

void ProcessDiceHeaderDelegateImpl::HandleTokenExchangeFailure(
    const std::string& email,
    const GoogleServiceAuthError& error) {
  DCHECK_NE(GoogleServiceAuthError::NONE, error.state());
  content::WebContents* web_contents = web_contents_.get();
  DiceTabHelper* tab_helper = GetDiceTabHelperFromWebContents(web_contents);
  if (tab_helper) {
    tab_helper->OnSyncSigninFlowComplete();
  }

  if (ShouldEnableSync()) {
    Redirect();
  }

  // Show the error even if the WebContents was closed, because the user may be
  // signed out of the web.
  std::move(show_signin_error_callback_)
      .Run(&profile_.get(), web_contents,
           SigninUIError::FromGoogleServiceAuthError(email, error));
}

signin_metrics::AccessPoint ProcessDiceHeaderDelegateImpl::GetAccessPoint() {
  return access_point_;
}

void ProcessDiceHeaderDelegateImpl::OnDiceSigninHeaderReceived() {
  // TODO(b/303612320): The check for the `DiceTabHelper` here is needed since
  // this is where we are getting the callback from and we will be redirected
  // when calling it.
  //
  // We should cut down this dependency by not depending directly on the
  // `DiceTabHelper` callback (this class receives a copy of the callback
  // through the constructor) but rather providing an intermediate callback that
  // would redirect to the proper one. This way the dependency would be direct
  // with `ProcessDiceHeaderDelegateImpl` and then from
  // `ProcessDiceHeaderDelegateImpl` to the `DiceTabHelper`.
  //
  // This should be done for the 3 callbacks in this class:
  // `EnableSyncCallback`, `ShowSigninErrorCallback` and
  // `OnSigninHeaderReceived`.
  DiceTabHelper* tab_helper =
      GetDiceTabHelperFromWebContents(web_contents_.get());
  if (!tab_helper) {
    return;
  }

  if (on_signin_header_received_) {
    std::move(on_signin_header_received_).Run();
  }
}

void ProcessDiceHeaderDelegateImpl::Redirect() {
  content::WebContents* web_contents = web_contents_.get();
  if (!web_contents || redirect_url_.is_empty()) {
    return;
  }

  DCHECK(redirect_url_.is_valid()) << "Invalid redirect url: " << redirect_url_;
  web_contents->GetController().LoadURL(redirect_url_, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
}
