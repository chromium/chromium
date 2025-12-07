// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {
constexpr char kDiceSyncHeaderTimeoutHistogramNameHistogramName[] =
    "Signin.SigninManager.SyncHeaderTimeout";
constexpr char kDiceSyncHeaderArrivalTimeWindowHistogramName[] =
    "Signin.SigninManager.SyncHeaderArrivalTimeWindowAfterLst";

void RecordDiceSyncHeaderTimeout(bool timeout) {
  base::UmaHistogramBoolean(kDiceSyncHeaderTimeoutHistogramNameHistogramName,
                            timeout);
}

}  // namespace

// static
base::TimeDelta DiceTabHelper::g_delay_before_interception_bubble_retry =
    base::Seconds(3);

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

    bool is_sync_promo =
        access_point ==
            signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo ||
        access_point == signin_metrics::AccessPoint::kSettings ||
        access_point == signin_metrics::AccessPoint::kSettingsYourSavedInfo;
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
DiceTabHelper::EnableHistorySyncOptinCallback
DiceTabHelper::GetHistorySyncOptinCallbackForBrowser() {
  return base::BindRepeating([](Profile* profile,
                                content::WebContents* web_contents,
                                const CoreAccountInfo& account_info,
                                signin_metrics::AccessPoint access_point) {
    CHECK(base::FeatureList::IsEnabled(
        syncer::kReplaceSyncPromosWithSignInPromos));
    CHECK(profile);

    Browser* browser = web_contents ? chrome::FindBrowserWithTab(web_contents)
                                    : chrome::FindBrowserWithProfile(profile);
    if (!browser) {
      return;
    }

    HistorySyncOptinService* history_sync_optin_service =
        HistorySyncOptinServiceFactory::GetForProfile(profile);
    CHECK(history_sync_optin_service);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    AccountInfo extended_account_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            account_info.account_id);
    if (extended_account_info.IsEmpty()) {
      return;
    }
    CHECK(identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .account_id == account_info.account_id);
    history_sync_optin_service->StartHistorySyncOptinFlow(
        extended_account_info,
        std::make_unique<HistorySyncOptinServiceDefaultDelegate>(),
        access_point);
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

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<DiceTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents),
      state_{std::make_unique<ResetableState>()} {}

DiceTabHelper::~DiceTabHelper() = default;

void DiceTabHelper::InitializeSigninFlow(
    const GURL& signin_url,
    signin_metrics::AccessPoint access_point,
    signin_metrics::Reason reason,
    signin_metrics::PromoAction promo_action,
    const GURL& redirect_url,
    bool record_signin_started_metrics,
    EnableSyncCallback enable_sync_callback,
    EnableHistorySyncOptinCallback history_sync_optin_callback,
    OnSigninHeaderReceived on_signin_header_received_callback,
    ShowSigninErrorCallback show_signin_error_callback) {
  DCHECK(signin_url.is_valid());
  DCHECK(state_->signin_url.is_empty() || state_->signin_url == signin_url);

  Reset();
  state_->redirect_url = redirect_url;
  state_->signin_url = signin_url;
  state_->signin_access_point = access_point;
  state_->signin_promo_action = promo_action;
  state_->signin_reason = reason;
  state_->enable_sync_callback = std::move(enable_sync_callback);
  state_->history_sync_optin_callback = std::move(history_sync_optin_callback);
  state_->on_signin_header_received_callback =
      std::move(on_signin_header_received_callback);
  state_->show_signin_error_callback = std::move(show_signin_error_callback);

  is_chrome_signin_page_ = true;
  signin_page_load_recorded_ = false;

  if (reason == signin_metrics::Reason::kSigninPrimaryAccount) {
    state_->sync_signin_flow_status = SyncSigninFlowStatus::kStarted;
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
        "Signin.SigninPending.ResolutionSourceStarted", access_point);
  }
}

bool DiceTabHelper::IsChromeSigninPage() const {
  return is_chrome_signin_page_;
}

bool DiceTabHelper::IsSyncSigninInProgress() const {
  return state_->sync_signin_flow_status == SyncSigninFlowStatus::kStarted;
}

void DiceTabHelper::OnSyncSigninFlowComplete() {
  // The flow is complete, reset to initial state.
  StopInterceptionBubbleTimer();
  Reset();
}

void DiceTabHelper::OnTokenExchangeSuccess(
    base::OnceClosure retry_interception_bubble_callback) {
  StartInterceptionBubbleTimer(std::move(retry_interception_bubble_callback));
}

void DiceTabHelper::UpdateSyncCallback(
    EnableSyncCallback enable_sync_callback) {
  state_->enable_sync_callback = std::move(enable_sync_callback);
}

void DiceTabHelper::UpdateHistorySyncOptinCallback(
    EnableHistorySyncOptinCallback history_sync_optin_callback) {
  state_->history_sync_optin_callback = std::move(history_sync_optin_callback);
}

void DiceTabHelper::UpdateSigninErrorCallback(
    ShowSigninErrorCallback show_signin_error_callback) {
  state_->show_signin_error_callback = std::move(show_signin_error_callback);
}

void DiceTabHelper::UpdateRedirectUrl(const GURL& redirect_url) {
  state_->redirect_url = redirect_url;
}

// static
base::AutoReset<base::TimeDelta>
DiceTabHelper::SetScopedInterceptionBubbleTimerForTesting(
    base::TimeDelta delay) {
  return base::AutoReset<base::TimeDelta>(
      &g_delay_before_interception_bubble_retry, delay);
}

// Below we elaborate into what actions take place between the LST reception and
// the `ENABLE_SYNC` header reception (if the latter occurs).
// There are 3 cases:
// 1) Chrome receives the Dice `ENABLE_SYNC` header without having received the
// LST. This is unexpected, and can only happen due to a server bug. The
// histogram `UnexpectedSyncHeaderProcessingBeforeLST` is recorded in that case.
// 2) Chrome receives the LST and then the `ENABLE_SYNC` header before a timeout
// set in `g_delay_before_interception_bubble_retry`. Chrome coordinates the
// handling of these events so that they are processed in the right order (first
// LST, the Sync header). The interception bubble is not shown, the histograms
// are `Signin.SigninManager.SyncHeaderTimeout` and
// `SyncHeaderArrivalTimeWindow` recorded and all timers are stopped when
// receiving the sync header. 3) Chrome receives the LST but does not receive
// the enable sync header within the `g_delay_before_interception_bubble_retry`
// timeout. Then the interception bubble is shown and the histogram
// `Signin.SigninManager.SyncHeaderTimeout` is recorded. The
// retry_interception_bubble_timer is stopped, but
// `elapsed_time_since_lst_arrival_timer` keeps running. If the enable sync
// header is later received, then `elapsed_time_since_lst_arrival_timer` is
// stopped and recorded in the histogram `SyncHeaderArrivalTimeWindow`.
void DiceTabHelper::StartInterceptionBubbleTimer(
    base::OnceClosure retry_interception_bubble_callback) {
  base::OnceClosure timer_callback =
      std::move(base::BindOnce(&RecordDiceSyncHeaderTimeout, true))
          .Then(std::move(retry_interception_bubble_callback));
  state_->elapsed_time_since_lst_arrival_timer =
      std::make_unique<base::ElapsedTimer>();
  state_->retry_interception_bubble_timer.Start(
      FROM_HERE, g_delay_before_interception_bubble_retry,
      std::move(timer_callback));
}

void DiceTabHelper::StopInterceptionBubbleTimer() {
  if (state_->retry_interception_bubble_timer.IsRunning()
      // Unexpected, edge case where a token exchange hasn't been requested yet
      // by the time Chrome processes the Sync header.
      || !IsTokenExchangeDone()) {
    RecordDiceSyncHeaderTimeout(false);
  }
  state_->retry_interception_bubble_timer.Stop();

  // Record metrics related to the reception of the Dice Sync header.
  if (!IsTokenExchangeDone()) {
    // In Chrome the processing of the Sync header is coordinated
    // to happen after the LST exchange (which initiates this timer),
    // if the Sync header arrives from Gaia before the LST exchange's
    // completion. Since Gaia is an external service, there is an unlikely edge
    // case that Gaia sends the Sync header before Chrome can properly
    // coordinate these events.
    base::UmaHistogramBoolean(
        "Signin.SigninManager.UnexpectedSyncHeaderProcessingBeforeLST", true);
    return;
  }
  auto elapsed_time = state_->elapsed_time_since_lst_arrival_timer->Elapsed();
  base::UmaHistogramMediumTimes(kDiceSyncHeaderArrivalTimeWindowHistogramName,
                                elapsed_time);
}

bool DiceTabHelper::IsTokenExchangeDone() const {
  return state_->elapsed_time_since_lst_arrival_timer.get();
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

  if (state_->signin_reason == signin_metrics::Reason::kSigninPrimaryAccount &&
      !signin_page_load_recorded_) {
    signin_page_load_recorded_ = true;
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Shown"));
  }
}

bool DiceTabHelper::IsSigninPageNavigation(
    content::NavigationHandle* navigation_handle) const {
  return !navigation_handle->IsErrorPage() &&
         navigation_handle->GetRedirectChain()[0] == state_->signin_url &&
         gaia::HasGaiaSchemeHostPort(navigation_handle->GetURL());
}

void DiceTabHelper::Reset() {
  state_ = std::make_unique<ResetableState>();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DiceTabHelper);
