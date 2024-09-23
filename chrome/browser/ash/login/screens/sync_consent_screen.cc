// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/sync_consent_screen.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_manager/user_manager.h"

namespace {

constexpr char kUserActionContinue[] = "continue";
constexpr char kUserActionLacrosSync[] = "sync-everything";
constexpr char kUserActionLacrosCustom[] = "sync-custom";
constexpr char kUserActionLacrosDecline[] = "lacros-decline";
// OS Sync type options
constexpr char kOsApps[] = "osApps";
constexpr char kOsPreferences[] = "osPreferences";
constexpr char kOsWifiConfigurations[] = "osWifiConfigurations";
constexpr char kOsWallpaper[] = "osWallpaper";

// This helper function to convert user selected items to UserSelectableOsType.
void GetUserSelectedSyncOsType(const base::Value::Dict& os_sync_items,
                               syncer::UserSelectableOsTypeSet& os_sync_set) {
  if (os_sync_items.FindBool(kOsApps).value()) {
    os_sync_set.Put(syncer::UserSelectableOsType::kOsApps);
  }
  if (os_sync_items.FindBool(kOsPreferences).value()) {
    os_sync_set.Put(syncer::UserSelectableOsType::kOsPreferences);
  }
  if (os_sync_items.FindBool(kOsWifiConfigurations).value()) {
    os_sync_set.Put(syncer::UserSelectableOsType::kOsWifiConfigurations);
  }
}

}  // namespace

namespace ash {
namespace {

// Delay showing chrome sync settings by this amount of time to make them
// show on top of the restored tabs and windows.
constexpr base::TimeDelta kSyncConsentSettingsShowDelay = base::Seconds(3);

constexpr base::TimeDelta kWaitTimeout = base::Seconds(10);
constexpr base::TimeDelta kWaitTimeoutForTest = base::Milliseconds(1);

std::optional<bool> sync_disabled_by_policy_for_test;
std::optional<bool> sync_engine_initialized_for_test;

SyncConsentScreen::SyncConsentScreenExitTestDelegate* test_exit_delegate_ =
    nullptr;

syncer::SyncService* GetSyncService(Profile* profile) {
  if (SyncServiceFactory::HasSyncService(profile))
    return SyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

void RecordUmaReviewFollowingSetup(bool value) {
  base::UmaHistogramBoolean("OOBE.SyncConsentScreen.ReviewFollowingSetup",
                            value);
}

// Returns true if the user is in minor mode (e.g. under age of 18). The value
// is read from account capabilities. We assume user is in minor mode if
// capability value is unknown.
bool IsMinorMode(Profile* profile, const user_manager::User* user) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  std::string gaia_id = user->GetAccountId().GetGaiaId();
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
  auto capability =
      account_info.capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions();
  base::UmaHistogramBoolean("OOBE.SyncConsentScreen.IsCapabilityKnown",
                            capability != signin::Tribool::kUnknown);
  return capability != signin::Tribool::kTrue;
}

base::TimeDelta GetWaitTimeout() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeTriggerSyncTimeoutForTests)) {
    return kWaitTimeoutForTest;
  }
  return kWaitTimeout;
}

}  // namespace

// static
std::string SyncConsentScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::DECLINE:
      return "DeclineOnLacros";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

// static
void SyncConsentScreen::MaybeLaunchSyncConsentSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(
          ::prefs::kShowSyncSettingsOnSessionStart)) {
    // TODO (alemate): In a very special case when chrome is exiting at the very
    // moment we show Settings, it might crash here because profile could be
    // already destroyed. This needs to be fixed.
    if (crosapi::browser_util::IsLacrosEnabled()) {
      profile->GetPrefs()->ClearPref(::prefs::kShowArcSettingsOnSessionStart);
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chromeos::settings::mojom::kSyncSetupSubpagePath);
    } else {
      // SyncSetupSubPage here is shown in the browser instead of the OS
      // Settings. We delay showing chrome sync settings by
      // kSyncConsentSettingsShowDelay to make the settings tab shows on top of
      // the restored tabs and windows.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](Profile* profile) {
                profile->GetPrefs()->ClearPref(
                    ::prefs::kShowSyncSettingsOnSessionStart);
                chrome::ShowSettingsSubPageForProfile(
                    profile, chrome::kSyncSetupSubPage);
              },
              base::Unretained(profile)),
          kSyncConsentSettingsShowDelay);
    }
  }
}

SyncConsentScreen::SyncConsentScreen(base::WeakPtr<SyncConsentScreenView> view,
                                     const ScreenExitCallback& exit_callback)
    : BaseScreen(SyncConsentScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

SyncConsentScreen::~SyncConsentScreen() = default;

void SyncConsentScreen::Init(const WizardContext& context) {
  if (is_initialized_)
    return;
  is_initialized_ = true;
  user_ = user_manager::UserManager::Get()->GetPrimaryUser();
  profile_ = ProfileHelper::Get()->GetProfileByUser(user_);
  UpdateScreen(context);
}

void SyncConsentScreen::Finish(Result result) {
  DCHECK(profile_);
  profile_->GetPrefs()->SetBoolean(prefs::kRecordArcAppSyncMetrics, true);
  // Always set completed, even if the dialog was skipped (e.g. by policy).
  profile_->GetPrefs()->SetBoolean(prefs::kSyncOobeCompleted, true);
  // Record whether the dialog was shown, skipped, etc.
  base::UmaHistogramEnumeration("OOBE.SyncConsentScreen.Behavior", behavior_);
  // Record the final state of the sync service.
  syncer::SyncService* service = GetSyncService(profile_);
  bool sync_enabled = service && service->IsSyncFeatureEnabled() &&
                      service->GetUserSettings()->IsSyncEverythingEnabled();
  base::UmaHistogramBoolean("OOBE.SyncConsentScreen.SyncEnabled", sync_enabled);
  if (test_exit_delegate_) {
    CHECK_IS_TEST();
    test_exit_delegate_->OnSyncConsentScreenExit(result, exit_callback_);
  } else {
    exit_callback_.Run(result);
  }
}

bool SyncConsentScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  Init(context);

  switch (behavior_) {
    case SyncScreenBehavior::kUnknown:
    case SyncScreenBehavior::kShow:
      return false;
    case SyncScreenBehavior::kSkipNonGaiaAccount:
    case SyncScreenBehavior::kSkipPublicAccount:
    case SyncScreenBehavior::kSkipPermissionsPolicy:
    case SyncScreenBehavior::kSkipAndEnableNonBrandedBuild:
    case SyncScreenBehavior::kSkipAndEnableEmphemeralUser:
    case SyncScreenBehavior::kSkipAndEnableScreenPolicy:
      MaybeEnableSyncForSkip();
      Finish(Result::NOT_APPLICABLE);
      return true;
  }
}

void SyncConsentScreen::ShowImpl() {
  Init(*context());

  if (behavior_ != SyncScreenBehavior::kShow) {
    syncer::SyncService* service = GetSyncService(profile_);
    if (service)
      sync_service_observation_.Observe(service);
    timeout_waiter_.Start(FROM_HERE, GetWaitTimeout(),
                          base::BindOnce(&SyncConsentScreen::OnTimeout,
                                         weak_factory_.GetWeakPtr()));
    start_time_ = base::TimeTicks::Now();
  } else {
    PrepareScreenBasedOnCapability();
    view_->ShowLoadedStep(IsOsSyncLacros());
  }

  // Show the entire screen.
  // If SyncScreenBehavior is show, this should show the sync consent screen.
  // If SyncScreenBehavior is unknown, this should show the loading throbber.
  if (view_) {
    view_->Show(crosapi::browser_util::IsLacrosEnabled());
  }

  if (context()->extra_factors_token) {
    session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
        context()->extra_factors_token.value());
  }
}

void SyncConsentScreen::HideImpl() {
  session_refresher_.reset();
  sync_service_observation_.Reset();
  timeout_waiter_.AbandonAndStop();
}

void SyncConsentScreen::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(context());
  UpdateScreen(*context());
}

void SyncConsentScreen::MaybeEnableSyncForSkip() {
  // "sync everything" toggle is disabled during SyncService creation. We need
  // to turn it on if sync service needs to be enabled.
  switch (behavior_) {
    case SyncScreenBehavior::kUnknown:
    case SyncScreenBehavior::kShow:
      NOTREACHED_IN_MIGRATION();
      return;
    case SyncScreenBehavior::kSkipNonGaiaAccount:
    case SyncScreenBehavior::kSkipPublicAccount:
    case SyncScreenBehavior::kSkipPermissionsPolicy:
      // Nothing to do.
      return;
    case SyncScreenBehavior::kSkipAndEnableNonBrandedBuild:
    case SyncScreenBehavior::kSkipAndEnableEmphemeralUser:
    case SyncScreenBehavior::kSkipAndEnableScreenPolicy:
      // Sync is autostarted during SyncService
      // creation with "sync everything" toggle off. We need to turn it on here.
      SetSyncEverythingEnabled(/*enabled=*/true);
      return;
  }
}

void SyncConsentScreen::OnTimeout() {
  is_timed_out_ = true;
  DCHECK(context());
  UpdateScreen(*context());
}

void SyncConsentScreen::SetDelegateForTesting(
    SyncConsentScreen::SyncConsentScreenTestDelegate* delegate) {
  test_delegate_ = delegate;
}

// static
void SyncConsentScreen::SetSyncConsentScreenExitTestDelegate(
    SyncConsentScreen::SyncConsentScreenExitTestDelegate* test_delegate) {
  test_exit_delegate_ = test_delegate;
}

SyncConsentScreen::SyncConsentScreenTestDelegate*
SyncConsentScreen::GetDelegateForTesting() const {
  return test_delegate_;
}

SyncConsentScreen::SyncScreenBehavior SyncConsentScreen::GetSyncScreenBehavior(
    const WizardContext& context) const {
  // Skip for users without Gaia account (e.g. Active Directory).
  if (!user_->HasGaiaAccount())
    return SyncScreenBehavior::kSkipNonGaiaAccount;

  // Skip for public user.
  if (user_->GetType() == user_manager::UserType::kPublicAccount) {
    return SyncScreenBehavior::kSkipPublicAccount;
  }

  // Skip for non-branded (e.g. developer) builds. Check this after the account
  // type checks so we don't try to enable sync in browser_tests for those
  // account types.
  if (!context.is_branded_build)
    return SyncScreenBehavior::kSkipAndEnableNonBrandedBuild;

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  // Skip for non-regular ephemeral users.
  if (user_manager->IsUserNonCryptohomeDataEphemeral(user_->GetAccountId()) &&
      (user_->GetType() != user_manager::UserType::kRegular)) {
    return SyncScreenBehavior::kSkipAndEnableEmphemeralUser;
  }

  // Skip if the sync consent screen is disabled by policy, for example, in
  // education scenarios. https://crbug.com/841156
  if (!profile_->GetPrefs()->GetBoolean(::prefs::kEnableSyncConsent))
    return SyncScreenBehavior::kSkipAndEnableScreenPolicy;

  // Skip if sync-the-feature is disabled by policy.
  if (IsProfileSyncDisabledByPolicy())
    return SyncScreenBehavior::kSkipPermissionsPolicy;

  if (IsProfileSyncEngineInitialized() || is_timed_out_)
    return SyncScreenBehavior::kShow;

  return SyncScreenBehavior::kUnknown;
}

void SyncConsentScreen::UpdateScreen(const WizardContext& context) {
  const SyncScreenBehavior new_behavior = GetSyncScreenBehavior(context);
  if (new_behavior == SyncScreenBehavior::kUnknown)
    return;

  const SyncScreenBehavior old_behavior = behavior_;
  behavior_ = new_behavior;

  if (is_hidden() || behavior_ == old_behavior)
    return;

  if (behavior_ == SyncScreenBehavior::kShow) {
    PrepareScreenBasedOnCapability();

    if (view_) {
      view_->ShowLoadedStep(IsOsSyncLacros());
    }
    GetSyncService(profile_)->RemoveObserver(this);
    timeout_waiter_.AbandonAndStop();
    base::UmaHistogramCustomTimes("OOBE.SyncConsentScreen.LoadingTime",
                                  base::TimeTicks::Now() - start_time_,
                                  base::Milliseconds(1), base::Seconds(10), 50);
  } else {
    MaybeEnableSyncForSkip();
    Finish(Result::NEXT);
  }
}

void SyncConsentScreen::RecordConsent(
    ConsentGiven consent_given,
    const std::vector<int>& consent_description,
    int consent_confirmation) {
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile_);
  // The user might not consent to browser sync, so use the "unconsented" ID.
  const CoreAccountId& google_account_id =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  // TODO(alemate): Support unified_consent_enabled
  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(consent_confirmation);
  for (int id : consent_description) {
    sync_consent.add_description_grd_ids(id);
  }
  sync_consent.set_status(consent_given == CONSENT_GIVEN
                              ? sync_pb::UserConsentTypes::GIVEN
                              : sync_pb::UserConsentTypes::NOT_GIVEN);
  consent_auditor->RecordSyncConsent(google_account_id, sync_consent);

  if (test_delegate_) {
    test_delegate_->OnConsentRecordedIds(consent_given, consent_description,
                                         consent_confirmation);
  }
}

bool SyncConsentScreen::IsProfileSyncDisabledByPolicyForTest() const {
  return sync_disabled_by_policy_for_test.has_value() &&
         sync_disabled_by_policy_for_test.value();
}

bool SyncConsentScreen::IsProfileSyncDisabledByPolicy() const {
  if (sync_disabled_by_policy_for_test.has_value())
    return sync_disabled_by_policy_for_test.value();
  const syncer::SyncService* sync_service = GetSyncService(profile_);
  return sync_service->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

bool SyncConsentScreen::IsProfileSyncEngineInitialized() const {
  if (sync_engine_initialized_for_test.has_value())
    return sync_engine_initialized_for_test.value();
  const syncer::SyncService* sync_service = GetSyncService(profile_);
  return sync_service->IsEngineInitialized();
}

void SyncConsentScreen::PrepareScreenBasedOnCapability() {
  bool is_minor_mode = IsMinorMode(profile_, user_);
  base::UmaHistogramBoolean("OOBE.SyncConsentScreen.IsMinorUser",
                            is_minor_mode);
  // Turn on "sync everything" toggle for non-minor users; turn off all data
  // types for minor users for the ash sync.
  if (!IsOsSyncLacros()) {
    SetSyncEverythingEnabled(!is_minor_mode);
  }

  if (view_) {
    view_->SetIsMinorMode(is_minor_mode);
  }
}

// Check if OSSyncRevamp and Lacros are enabled.
bool SyncConsentScreen::IsOsSyncLacros() {
  return crosapi::browser_util::IsLacrosEnabled() &&
         features::IsOsSyncConsentRevampEnabled();
}

void SyncConsentScreen::SetSyncEverythingEnabled(bool enabled) {
  syncer::SyncService* sync_service = GetSyncService(profile_);
  syncer::SyncUserSettings* sync_settings = sync_service->GetUserSettings();
  if (enabled != sync_settings->IsSyncEverythingEnabled()) {
    syncer::UserSelectableTypeSet empty_set;
    sync_settings->SetSelectedTypes(enabled, empty_set);
  }

  if (enabled != sync_settings->IsSyncAllOsTypesEnabled()) {
    syncer::UserSelectableOsTypeSet os_empty_set;
    sync_settings->SetSelectedOsTypes(enabled, os_empty_set);
  }
}

// static
void SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(bool value) {
  sync_disabled_by_policy_for_test = value;
}

// static
void SyncConsentScreen::SetProfileSyncEngineInitializedForTesting(bool value) {
  sync_engine_initialized_for_test = value;
}

// todo(b/283119955) align with browser record sync
void SyncConsentScreen::OnAshContinue(
    const bool opted_in,
    const bool review_sync,
    const base::Value::List& consent_description_list,
    const std::string& consent_confirmation) {
  if (!view_ || is_hidden()) {
    return;
  }

  RecordUmaReviewFollowingSetup(review_sync);
  base::UmaHistogramEnumeration(
      "OOBE.SyncConsentScreen.UserChoice",
      opted_in ? SyncConsentScreenHandler::UserChoice::kAccepted
               : SyncConsentScreenHandler::UserChoice::kDeclined);
  profile_->GetPrefs()->SetBoolean(::prefs::kShowSyncSettingsOnSessionStart,
                                   review_sync);
  SetSyncEverythingEnabled(opted_in);
  RecordAllConsents(opted_in, consent_description_list, consent_confirmation);
  Finish(Result::NEXT);
}

void SyncConsentScreen::RecordAllConsents(
    const bool opted_in,
    const base::Value::List& consent_description_list,
    const std::string& consent_confirmation) {
  auto consent_description =
      ::login::ConvertToStringList(consent_description_list);
  std::vector<int> consent_description_ids;
  int consent_confirmation_id;
  if (view_) {
    view_->RetrieveConsentIDs(consent_description, consent_confirmation,
                              consent_description_ids, consent_confirmation_id);
    RecordConsent(opted_in ? CONSENT_GIVEN : CONSENT_NOT_GIVEN,
                  consent_description_ids, consent_confirmation_id);
  }
  // IN-TEST
  SyncConsentScreen::SyncConsentScreenTestDelegate* test_delegate =
      GetDelegateForTesting();  // IN-TEST
  if (test_delegate) {
    CHECK_IS_TEST();
    test_delegate->OnConsentRecordedStrings(consent_description,
                                            consent_confirmation);
  }
}

void SyncConsentScreen::OnLacrosContinue(
    const base::Value::List& consent_description_list,
    const std::string& consent_confirmation) {
  RecordAllConsents(/*opted_in=*/true, consent_description_list,
                    consent_confirmation);
}

void SyncConsentScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinue) {
    CHECK_EQ(args.size(), 5u);
    const bool opted_in = args[1].GetBool();
    const bool review_sync = args[2].GetBool();
    const base::Value::List& consent_description_list = args[3].GetList();
    const std::string& consent_confirmation = args[4].GetString();
    OnAshContinue(opted_in, review_sync, consent_description_list,
                  consent_confirmation);
    return;
  }
  if (action_id == kUserActionLacrosSync) {
    CHECK_EQ(args.size(), 3u);

    const base::Value::List& consent_description_list = args[1].GetList();
    const std::string& consent_confirmation = args[2].GetString();

    OnLacrosContinue(consent_description_list, consent_confirmation);

    syncer::SyncService* sync_service = GetSyncService(profile_);
    syncer::SyncUserSettings* sync_settings = sync_service->GetUserSettings();

    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.SyncEverything", true);

    syncer::UserSelectableOsTypeSet os_empty_set;
    sync_settings->SetSelectedOsTypes(/*sync_all_os_types=*/true, os_empty_set);

    if (test_exit_delegate_) {
      CHECK_IS_TEST();
      test_exit_delegate_->OnSyncConsentScreenExit(Result::NEXT,
                                                   exit_callback_);
    } else {
      exit_callback_.Run(Result::NEXT);
    }

    return;
  }
  if (action_id == kUserActionLacrosDecline) {
    CHECK_EQ(args.size(), 1u);
    syncer::SyncService* sync_service = GetSyncService(profile_);
    syncer::SyncUserSettings* sync_settings = sync_service->GetUserSettings();

    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.SyncEverything", false);

    syncer::UserSelectableOsTypeSet os_empty_set;
    sync_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                      os_empty_set);

    if (test_exit_delegate_) {
      CHECK_IS_TEST();
      test_exit_delegate_->OnSyncConsentScreenExit(Result::DECLINE,
                                                   exit_callback_);
    } else {
      exit_callback_.Run(Result::DECLINE);
    }
    return;
  }
  if (action_id == kUserActionLacrosCustom) {
    CHECK_EQ(args.size(), 4u);
    const base::Value::Dict& osSyncItemsStatus = args[1].GetDict();
    syncer::UserSelectableOsTypeSet os_sync_set;

    const base::Value::List& consent_description_list = args[2].GetList();
    const std::string& consent_confirmation = args[3].GetString();

    OnLacrosContinue(consent_description_list, consent_confirmation);

    GetUserSelectedSyncOsType(osSyncItemsStatus, os_sync_set);

    syncer::SyncService* sync_service = GetSyncService(profile_);
    syncer::SyncUserSettings* sync_settings = sync_service->GetUserSettings();

    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.SyncEverything", false);

    sync_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false, os_sync_set);

    bool wallpaper_synced = osSyncItemsStatus.FindBool(kOsWallpaper).value();

    if (wallpaper_synced) {
      DCHECK(osSyncItemsStatus.FindBool(kOsPreferences).value());
    }

    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncWallpaper",
        wallpaper_synced);
    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncApps",
        osSyncItemsStatus.FindBool(kOsApps).value());
    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncSettings",
        osSyncItemsStatus.FindBool(kOsPreferences).value());
    base::UmaHistogramBoolean(
        "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncWifi",
        osSyncItemsStatus.FindBool(kOsWifiConfigurations).value());
    profile_->GetPrefs()->SetBoolean(settings::prefs::kSyncOsWallpaper,
                                     wallpaper_synced);

    if (test_exit_delegate_) {
      CHECK_IS_TEST();
      test_exit_delegate_->OnSyncConsentScreenExit(Result::NEXT,
                                                   exit_callback_);
    } else {
      exit_callback_.Run(Result::NEXT);
    }

    return;
  }
  BaseScreen::OnUserAction(args);
}
}  // namespace ash
