// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"

#include <memory>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signin/managed_profile_creation_controller.h"
#include "chrome/browser/enterprise/signin/signals_disclaimer_metrics.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "url/gurl.h"

namespace {

// TODO(b/537182192): Replace with a P-link.
constexpr char kSignalsDisclaimerLearnMoreURL[] =
    "https://support.google.com/chrome/a/answer/16191236";

bool CanTryPolicyRegistration(std::optional<base::Time> last_failure_time) {
  if (!last_failure_time) {
    return true;
  }

  return base::Time::Now() - last_failure_time.value() >
         switches::kPolicyDisclaimerRegistrationRetryDelay.Get();
}

bool IsSigninRegistration(signin_metrics::AccessPoint access_point) {
  return access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAtStartup &&
         access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAfterBrowserFocus;
}

bool AllowDisclaimer(signin_metrics::AccessPoint access_point) {
  if (base::FeatureList::IsEnabled(switches::kEnforceManagementDisclaimer)) {
    return true;
  }
  return access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAtStartup &&
         access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAfterBrowserFocus &&
         access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAfterSignin;
}

}  // namespace

ProfileManagementDisclaimerService::ProfileManagementDisclaimerService(
    Profile* profile)
    : profile_(*profile),
      state_(std::make_unique<ResetableState>()),
      signin_prefs_(*profile->GetPrefs()) {
  if (base::FeatureList::IsEnabled(
          policy::features::kDeviceSignalsBackfillDisclaimer) &&
      policy::features::kClearDeviceSignalsPermissionOnStartup.Get()) {
    profile->GetPrefs()->SetBoolean(
        device_signals::prefs::kDeviceSignalsPermanentConsentReceived, false);
  }

  scoped_identity_manager_observation_.Observe(GetIdentityManager());
  auto* browser_collection = ProfileBrowserCollection::GetForProfile(profile);
  if (browser_collection) {
    scoped_browser_collection_observation_.Observe(browser_collection);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ProfileManagementDisclaimerService::
                                    MaybeShowEnterpriseManagementDisclaimer,
                                weak_ptr_factory_.GetWeakPtr(),
                                GetPrimaryAccountInfo().GetAccountId(),
                                signin_metrics::AccessPoint::
                                    kEnterpriseManagementDisclaimerAtStartup));
}

ProfileManagementDisclaimerService::~ProfileManagementDisclaimerService() =
    default;

base::ScopedClosureRunner
ProfileManagementDisclaimerService::DisableManagementDisclaimerUntilReset() {
  if (!enable_management_disclaimer_) {
    return base::ScopedClosureRunner();
  }
  enable_management_disclaimer_ = false;
  return base::ScopedClosureRunner(
      base::BindOnce(&ProfileManagementDisclaimerService::
                         SetEnableManagementDisclaimerOnPrimaryAccountChange,
                     weak_ptr_factory_.GetWeakPtr(), true));
}

base::ScopedClosureRunner
ProfileManagementDisclaimerService::AutoAcceptManagementDisclaimerUntilReset() {
  active_auto_accept_count_++;
  auto_accept_management_ = true;
  return base::ScopedClosureRunner(base::BindOnce(
      &ProfileManagementDisclaimerService::MaybeResetAcceptManagementDisclaimer,
      weak_ptr_factory_.GetWeakPtr(), /*auto_accept_management=*/false));
}

void ProfileManagementDisclaimerService::MaybeResetAcceptManagementDisclaimer(
    bool auto_accept_management) {
  active_auto_accept_count_--;
  CHECK_GE(active_auto_accept_count_, 0);
  if (active_auto_accept_count_ == 0) {
    auto_accept_management_ = auto_accept_management;
  }
}

ProfileManagementDisclaimerService::ResetableState::ResetableState() = default;

ProfileManagementDisclaimerService::ResetableState::~ResetableState() {
  callbacks.Notify(profile_to_continue_in.get(),
                   profile_creation_required_by_policy);
}

void ProfileManagementDisclaimerService::EnsureManagedProfileForAccount(
    const CoreAccountId& account_id,
    signin_metrics::AccessPoint access_point,
    base::OnceCallback<void(Profile*, bool)> callback) {
  CHECK(state_->account_id.empty() || state_->account_id == account_id);
  state_->callbacks.AddUnsafe(std::move(callback));
  state_->cancelable = false;
  MaybeShowEnterpriseManagementDisclaimer(account_id, access_point);
}

const CoreAccountId& ProfileManagementDisclaimerService::
    GetAccountBeingConsideredForManagementIfAny() const {
  return state_->account_id;
}

bool ProfileManagementDisclaimerService::StopCurrentProcessIfPossible() {
  if (state_->profile_creation_controller) {
    return false;
  }
  if (!state_->cancelable) {
    return false;
  }
  Reset();
  return true;
}

signin::IdentityManager*
ProfileManagementDisclaimerService::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(&profile_.get());
}

AccountInfo ProfileManagementDisclaimerService::GetPrimaryAccountInfo() {
  auto* identity_manager = GetIdentityManager();
  CHECK(identity_manager);
  return identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
}

AccountInfo ProfileManagementDisclaimerService::GetExtendedAccountInfo(
    const CoreAccountId& account_id) {
  auto* identity_manager = GetIdentityManager();
  CHECK(identity_manager);
  return identity_manager->FindExtendedAccountInfoByAccountId(account_id);
}

void ProfileManagementDisclaimerService::
    MaybeShowEnterpriseManagementDisclaimer(
        const CoreAccountId& account_id,
        signin_metrics::AccessPoint access_point) {
  if (account_id.empty()) {
    return;
  }

  if (!AllowDisclaimer(access_point)) {
    return;
  }

  if (!state_->account_id.empty() && state_->account_id != account_id) {
    // If the account is different from the one we are already handling, reset
    // the state. This can happen if the account is removed and another one is
    // added, or if the account is cleared and another account is set as primary
    // account.
    return;
  }

  // If the management disclaimer is not enabled on primary account change,
  // reset the state and return early. This to avoid showing the disclaimer
  // after the primary account has changed when another class is handling
  // signin.
  if (!enable_management_disclaimer_) {
    Reset();
    return;
  }
  state_->access_point = access_point;

  // Wait for the current disclaimer to be closed.
  if (state_->profile_creation_controller) {
    return;
  }

  state_->account_id = account_id;

  // If the user has already accepted the management disclaimer, nothing to
  // show.
  if (enterprise_util::UserAcceptedAccountManagement(&profile_.get())) {
    state_->profile_to_continue_in = profile_->GetWeakPtr();
    Reset();
    return;
  }

  AccountInfo info = GetExtendedAccountInfo(account_id);

  // Account info is not yet available, wait for extended account info.
  if (info.IsManaged() == signin::Tribool::kUnknown) {
    state_->extended_account_info_wait_timeout.Start(
        FROM_HERE, base::Seconds(5),
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If there is no refresh token, we cannot register for policy updates.
  // Wait for it to be updated.
  if (!GetIdentityManager()->HasAccountWithRefreshToken(account_id)) {
    state_->refresh_token_wait_timeout.Start(
        FROM_HERE, base::Seconds(5),
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Account not managed, nothing to do.
  if (!signin::TriboolToBoolOrDie(info.IsManaged())) {
    Reset();
    return;
  }

  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(&profile_.get())
          ->GetLastActiveBrowser();
  bool has_browser_with_tab =
      browser && WindowFeatureController::From(browser)->SupportsWindowFeature(
                     WindowFeatureController::WindowFeature::kFeatureTabStrip);
  // If there is no browser and we are not in tests, abort.
  if (!has_browser_with_tab && !profile_separation_policies_for_testing_ &&
      !user_choice_for_testing_) {
    Reset();
    return;
  }

  CHECK(!state_->profile_creation_controller);

  // If the account cannot try to register for policies because of delays
  // between failures, we can reset the state and wait for another attempt.
  if (!CanTryPolicyRegistration(
          signin_prefs_.GetPolicyDisclaimerLastRegistrationFailureTime(
              info.GetGaiaId()))) {
    OnRegisteredForPolicy(/*is_from_cached_registration_result=*/true,
                          /*is_managed_account=*/false);
    return;
  }

  // If the account is already registered for policy, we can check the result
  // immediately. Otherwise, we need to register for policy updates.
  bool has_cached_successful_registration_result =
      policy_fetch_tracker_by_account_id_.contains(account_id) &&
      policy_fetch_tracker_by_account_id_[account_id]
          ->GetPolicyRegistrationResult()
          .value_or(false);

  if (has_cached_successful_registration_result) {
    OnRegisteredForPolicy(/*is_from_cached_registration_result=*/true,
                          /*is_managed_account=*/true);
    return;
  }

  // Create a new tracker for the account, if it doesn't exist yet or if it had
  // a cached failure. This will also reset any cached failure.
  policy_fetch_tracker_by_account_id_[account_id] =
      TurnSyncOnHelperPolicyFetchTracker::CreateInstance(&profile_.get(), info);

  policy_fetch_tracker_by_account_id_[account_id]->RegisterForPolicy(
      base::BindOnce(&ProfileManagementDisclaimerService::OnRegisteredForPolicy,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_from_cached_registration_result=*/false),
      !IsSigninRegistration(*state_->access_point));
}

bool ProfileManagementDisclaimerService::IsDeviceSignalsDisclaimerRequired(
    BrowserWindowInterface* browser) const {
  if (!base::FeatureList::IsEnabled(
          policy::features::kDeviceSignalsBackfillDisclaimer)) {
    return false;
  }

  // Suppress the dialog if we force --no-first-run for testing
  // and benchmarking.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoFirstRun) &&
      !bypass_no_first_run_) {
    return false;
  }

  // If the user has not accepted the account management yet,
  // they will see this disclaimer as part of that process in the future.
  if (!enterprise_util::UserAcceptedAccountManagement(&*profile_)) {
    return false;
  }

  // If the permission was already obtained the dialog is not necessary.
  if (profile_->GetPrefs()->GetBoolean(
          device_signals::prefs::kDeviceSignalsPermanentConsentReceived)) {
    return false;
  }

  // Browsers hosting the privacy article should not be blocked by the
  // disclaimer. `browser` can be nullptr when this is called by the profile
  // picker.
  if (browser && browser == privacy_article_browser_.get()) {
    return false;
  }

  return true;
}

void ProfileManagementDisclaimerService::MaybeShowDeviceSignalsDisclaimerDialog(
    BrowserWindowInterface* browser) {
  if (!IsDeviceSignalsDisclaimerRequired(browser)) {
    return;
  }

  // A tab must be active to present a modal dialog. We'll back off and try
  // again once the next OnBrowserActivated is fired.
  if (browser && !browser->GetActiveTabInterface()) {
    base::UmaHistogramEnumeration(
        kEnterpriseSignalsDisclaimerNotShownReason,
        EnterpriseSignalsDisclaimerNotShownReason::kTabsNotReady);
    return;
  }

  // Profile creation is already in progress.
  if (state_ && state_->profile_creation_controller) {
    base::UmaHistogramEnumeration(
        kEnterpriseSignalsDisclaimerNotShownReason,
        EnterpriseSignalsDisclaimerNotShownReason::kProfileCreationInProgress);
    return;
  }

  // The management notice dialog or another modal dialog is already open.
  if (browser->GetFeatures().signin_view_controller()->ShowsModalDialog()) {
    base::UmaHistogramEnumeration(
        kEnterpriseSignalsDisclaimerNotShownReason,
        EnterpriseSignalsDisclaimerNotShownReason::kOtherModalDialogShown);
    return;
  }

  base::UmaHistogramBoolean(kEnterpriseSignalsDisclaimerModalShown, true);

  browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(
          signin::EnterpriseProfileCreationDialogParams::
              CreateForDeviceSignalsDisclaimer(
                  GetPrimaryAccountInfo(),
                  base::BindOnce(&ProfileManagementDisclaimerService::
                                     HandleDeviceSignalsDisclaimerChoice,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 browser->GetWeakPtr()),
                  /*is_modal_dialog=*/true));
  opened_device_signals_disclaimers_.push_back(browser->GetWeakPtr());
}

void ProfileManagementDisclaimerService::HandleDeviceSignalsDisclaimerChoice(
    base::WeakPtr<BrowserWindowInterface> source_browser,
    signin::DeviceSignalsDisclaimerResult result) {
  switch (result) {
    case signin::DeviceSignalsDisclaimerResult::kAccepted: {
      base::UmaHistogramEnumeration(
          kEnterpriseSignalsDisclaimerModalResult,
          EnterpriseSignalsDisclaimerModalResult::kAccepted);
      // Close the dialog on all windows it was open and mark the permission as
      // granted.
      OnDeviceSignalsCollectionConsentGranted();

      auto browsers_to_close = std::move(opened_device_signals_disclaimers_);
      for (const auto& browser : browsers_to_close) {
        if (browser) {
          // This will trigger `HandleDeviceSignalsDisclaimerChoice` with
          // `kDismissed` for any other dialogs.
          browser->GetFeatures().signin_view_controller()->CloseModalSignin();
        }
      }

      break;
    }
    case signin::DeviceSignalsDisclaimerResult::kCanceled:
      base::UmaHistogramEnumeration(
          kEnterpriseSignalsDisclaimerModalResult,
          EnterpriseSignalsDisclaimerModalResult::kDeclined);
      // If the user does not grant permission all windows for this profile
      // should be closed and the profile picker should be presented.
      //
      // If the dialog was also opened in another window this function will be
      // called with kDismissed for each such dialog.
      opened_device_signals_disclaimers_.clear();
      chrome::CloseAllBrowsersWithProfile(&profile_.get());
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
      break;
    case signin::DeviceSignalsDisclaimerResult::kDismissed:
      // This case means the dialog in `source_browser` was not closed by
      // choosing either of the dialog buttons. If the dialog is missing from
      // `opened_device_signals_disclaimers_` it had to be closed explicitly by
      // either `kAccepted` or `kCanceled` branch.
      auto itr = std::find_if(opened_device_signals_disclaimers_.begin(),
                              opened_device_signals_disclaimers_.end(),
                              [&source_browser](const auto& browser) {
                                return browser && source_browser &&
                                       browser.get() == source_browser.get();
                              });
      if (itr != opened_device_signals_disclaimers_.end()) {
        base::UmaHistogramEnumeration(kEnterpriseSignalsDisclaimerModalResult,
                                      EnterpriseSignalsDisclaimerModalResult::
                                          kDismissedWithoutExplicitUserAction);
        opened_device_signals_disclaimers_.erase(itr);
      } else {
        base::UmaHistogramEnumeration(
            kEnterpriseSignalsDisclaimerModalResult,
            EnterpriseSignalsDisclaimerModalResult::kDismissedByAnotherWindow);
      }
      break;
  }
}

void ProfileManagementDisclaimerService::OnRegisteredForPolicy(
    bool is_from_cached_registration_result,
    bool is_managed_account) {
  if (!enable_management_disclaimer_) {
    Reset();
    return;
  }
  GaiaId gaia_id = GetExtendedAccountInfo(state_->account_id).GetGaiaId();
  // If the account has been removed in the meantime, reset the state.
  if (gaia_id.empty()) {
    state_->profile_to_continue_in = nullptr;
    Reset();
    return;
  }
  if (!is_managed_account) {
    if (!is_from_cached_registration_result) {
      signin_prefs_.SetPolicyDisclaimerLastRegistrationFailureTime(
          gaia_id, base::Time::Now());
    }
    Reset();
    return;
  }
  signin_prefs_.ClearPolicyDisclaimerLastRegistrationFailureTime(gaia_id);

  if (auto_accept_management_) {
    enterprise_util::SetUserAcceptedAccountManagement(&profile_.get(), true);
    OnManagedProfileCreationResult(
        base::ok<Profile*>(&profile_.get()),
        /*profile_creation_required_by_policy=*/false);
    return;
  }

  if (profile_separation_policies_for_testing_.has_value() ||
      user_choice_for_testing_.has_value()) {
    CHECK_IS_TEST();
    state_->profile_creation_controller =
        ManagedProfileCreationController::CreateManagedProfileForTesting(
            &profile_.get(), GetExtendedAccountInfo(state_->account_id),
            // The access point always has a value if the account_id is set.
            *state_->access_point,
            base::BindOnce(&ProfileManagementDisclaimerService::
                               OnManagedProfileCreationResult,
                           weak_ptr_factory_.GetWeakPtr()),
            std::move(profile_separation_policies_for_testing_),
            std::move(user_choice_for_testing_));
    return;
  }

  state_->profile_creation_controller =
      ManagedProfileCreationController::CreateManagedProfile(
          &profile_.get(), GetExtendedAccountInfo(state_->account_id),
          // The access point always has a value if the account_id is set.
          *state_->access_point,
          base::BindOnce(&ProfileManagementDisclaimerService::
                             OnManagedProfileCreationResult,
                         weak_ptr_factory_.GetWeakPtr()));
}

void ProfileManagementDisclaimerService::OnManagedProfileCreationResult(
    base::expected<Profile*, ManagedProfileCreationFailureReason> result,
    bool profile_creation_required_by_policy) {
  if (result.has_value() && result.value()) {
    state_->profile_to_continue_in = result.value()->GetWeakPtr();
  }
  state_->profile_creation_required_by_policy =
      profile_creation_required_by_policy;
  auto& policy_fetch_tracker =
      policy_fetch_tracker_by_account_id_[state_->account_id];
  if (state_->profile_to_continue_in && policy_fetch_tracker) {
    policy_fetch_tracker->SwitchToProfile(state_->profile_to_continue_in.get());
    policy_fetch_tracker->FetchPolicy(
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  Reset();
}

void ProfileManagementDisclaimerService::Reset() {
  state_ = std::make_unique<ResetableState>();
}

void ProfileManagementDisclaimerService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
          signin::PrimaryAccountChangeEvent::Type::kCleared &&
      state_->account_id == GetPrimaryAccountInfo().GetAccountId()) {
    state_->profile_to_continue_in = nullptr;
    Reset();
    return;
  }
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }

  // If we are already handling a signin, ignore this event.
  if (!state_->account_id.empty()) {
    return;
  }

  // Post the task here because the class that set the primary account might
  // handle the signin in a synchronous way. This avoids showing the disclaimer
  // twice.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProfileManagementDisclaimerService::
                         MaybeShowEnterpriseManagementDisclaimer,
                     weak_ptr_factory_.GetWeakPtr(),
                     event.GetCurrentState().primary_account.account_id,
                     signin_metrics::AccessPoint::
                         kEnterpriseManagementDisclaimerAfterSignin));
}

void ProfileManagementDisclaimerService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.GetAccountId() != state_->account_id) {
    return;
  }
  // Management status is not yet available, wait for extended account info.
  if (info.IsManaged() == signin::Tribool::kUnknown) {
    return;
  }
  state_->extended_account_info_wait_timeout.Stop();
  // The access point always has a value if the account_id is set.
  MaybeShowEnterpriseManagementDisclaimer(state_->account_id,
                                          *state_->access_point);
}

void ProfileManagementDisclaimerService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // This would most likely happen at startup after all refresh tokens are
  // loaded.
  if (state_->account_id.empty() &&
      GetPrimaryAccountInfo().GetAccountId() != account_info.account_id) {
    return;
  }
  if (!state_->account_id.empty() &&
      account_info.account_id != state_->account_id) {
    return;
  }
  MaybeShowEnterpriseManagementDisclaimer(
      account_info.account_id,
      state_->access_point.value_or(
          signin_metrics::AccessPoint::
              kEnterpriseManagementDisclaimerAfterSignin));
  state_->refresh_token_wait_timeout.Stop();
}

void ProfileManagementDisclaimerService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  MaybeShowDeviceSignalsDisclaimerDialog(browser);

  CoreAccountId account_id = state_->account_id.empty()
                                 ? GetPrimaryAccountInfo().GetAccountId()
                                 : state_->account_id;
  signin_metrics::AccessPoint access_point = state_->access_point.value_or(
      signin_metrics::AccessPoint::
          kEnterpriseManagementDisclaimerAfterBrowserFocus);
  MaybeShowEnterpriseManagementDisclaimer(account_id, access_point);
}

void ProfileManagementDisclaimerService::
    OnDeviceSignalsCollectionConsentGranted() {
  profile_->GetPrefs()->SetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived, true);
}

void ProfileManagementDisclaimerService::OpenPrivacyPolicyArticlePopUp(
    bool is_modal_dialog) {
  if (is_modal_dialog) {
    base::UmaHistogramBoolean(kEnterpriseSignalsDisclaimerModalLearnMoreClicked,
                              true);
  } else {
    base::UmaHistogramBoolean(
        kEnterpriseSignalsDisclaimerProfilePickerLearnMoreClicked, true);
  }

  // If the dedicated browser for the privacy article has already been created
  // bring it to focus instead of creating a new window.
  if (privacy_article_browser_) {
    if (auto* window = privacy_article_browser_->GetWindow()) {
      window->Show();
      window->Activate();
      return;
    } else {
      privacy_article_browser_.reset();
    }
  }

  // This will open a new browser window in the same profile. We need to make
  // sure the dialog is not shown in that window to allow the user to read the
  // article.
  BrowserWindowCreateParams create_params(BrowserWindowInterface::TYPE_POPUP,
                                          &*profile_,
                                          /*from_user_gesture=*/true);
  create_params.should_trigger_session_restore = false;
  create_params.omit_from_session_restore = true;
  BrowserWindowInterface* popup_browser =
      CreateBrowserWindow(std::move(create_params));
  if (popup_browser) {
    privacy_article_browser_ = popup_browser->GetWeakPtr();
    chrome::AddSelectedTabWithURL(popup_browser,
                                  GURL(kSignalsDisclaimerLearnMoreURL),
                                  ui::PAGE_TRANSITION_LINK);
    popup_browser->GetWindow()->Show();
    popup_browser->GetWindow()->Activate();
  }
}
