// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/i18n/timezone.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

using ArcBackupAndRestoreConsent =
    ::sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent;
using ArcGoogleLocationServiceConsent =
    ::sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent;
using ArcPlayTermsOfServiceConsent =
    ::sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using ::sync_pb::UserConsentTypes;

constexpr const char kBackDemoButtonClicked[] = "back";
constexpr const char kAcceptButtonClicked[] = "tos-accept";

enum class ToS { GOOGLE_EULA, CROS_EULA, ARC, PRIVACY_POLICY };

static constexpr auto kTermsTypeToUrlAndSwitch =
    base::MakeFixedFlatMap<ToS, std::pair<const char*, const char*>>(
        {{ToS::GOOGLE_EULA,
          {chrome::kGoogleEulaOnlineURLPath, switches::kOobeEulaUrlForTests}},
         {ToS::CROS_EULA,
          {chrome::kCrosEulaOnlineURLPath, switches::kOobeEulaUrlForTests}},
         {ToS::ARC,
          {chrome::kArcTosOnlineURLPath, switches::kArcTosHostForTests}},
         {ToS::PRIVACY_POLICY,
          {chrome::kPrivacyPolicyOnlineURLPath,
           switches::kPrivacyPolicyHostForTests}}});

std::string GetTosHost(ToS terms_type) {
  const char* ash_switch = kTermsTypeToUrlAndSwitch.at(terms_type).second;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(ash_switch)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        ash_switch);
  }

  const char* url_path = kTermsTypeToUrlAndSwitch.at(terms_type).first;
  if (terms_type == ToS::GOOGLE_EULA || terms_type == ToS::CROS_EULA) {
    return base::StringPrintfNonConstexpr(
        url_path, g_browser_process->GetApplicationLocale().c_str());
  }
  return url_path;
}

ConsolidatedConsentScreen::RecoveryOptInResult GetRecoveryOptInResult(
    const WizardContext::RecoverySetup& recovery_setup) {
  if (!recovery_setup.is_supported)
    return ConsolidatedConsentScreen::RecoveryOptInResult::kNotSupported;

  if (recovery_setup.ask_about_recovery_consent) {
    // The user was shown the opt-in checkbox.
    if (recovery_setup.recovery_factor_opted_in) {
      return ConsolidatedConsentScreen::RecoveryOptInResult::kUserOptIn;
    }
    return ConsolidatedConsentScreen::RecoveryOptInResult::kUserOptOut;
  }

  // The user was not shown the opt-in checkbox. In this case the policy value
  // is used.
  if (recovery_setup.recovery_factor_opted_in)
    return ConsolidatedConsentScreen::RecoveryOptInResult::kPolicyOptIn;
  return ConsolidatedConsentScreen::RecoveryOptInResult::kPolicyOptOut;
}

void RecordRecoveryOptinResult(
    const WizardContext::RecoverySetup& recovery_setup) {
  base::UmaHistogramEnumeration(
      "OOBE.ConsolidatedConsentScreen.RecoveryOptInResult",
      GetRecoveryOptInResult(recovery_setup));
}

}  // namespace

std::string ConsolidatedConsentScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::ACCEPTED:
      return "AcceptedRegular";
    case Result::ACCEPTED_DEMO_ONLINE:
      return "AcceptedDemo";
    case Result::BACK_DEMO:
      return "BackDemo";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

ConsolidatedConsentScreen::ConsolidatedConsentScreen(
    base::WeakPtr<ConsolidatedConsentScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ConsolidatedConsentScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

ConsolidatedConsentScreen::~ConsolidatedConsentScreen() {
  for (auto& observer : observer_list_)
    observer.OnConsolidatedConsentScreenDestroyed();
}

bool ConsolidatedConsentScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    StartupUtils::MarkEulaAccepted();

    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (!context.is_branded_build) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (arc::IsArcDemoModeSetupFlow())
    return false;

  if (user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // Do not skip the screen, if the system location toggle can be configured by
  // the active user. Returns false when the Privacy Hub Location is disabled.
  if (ash::privacy_hub_util::IsCrosLocationOobeNegotiationNeeded()) {
    return false;
  }

  // Admins are required to accept ToS on the server side.
  // So, if the profile is affiliated and arc negotiation is not needed, skip
  // the screen.

  // Do not skip the screen if ARC negotiaition is needed.
  if (arc::IsArcTermsOfServiceOobeNegotiationNeeded())
    return false;

  // Skip the screen if the user is affiliated.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  if (enterprise_util::IsProfileAffiliated(profile)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void ConsolidatedConsentScreen::ShowImpl() {
  if (!view_)
    return;

  is_child_account_ = user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  // TODO(b/326605902): Evaluate whether ownership status callback is needed.
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::BindOnce(&ConsolidatedConsentScreen::OnOwnershipStatusCheckDone,
                     weak_factory_.GetWeakPtr()));

  base::Value::Dict data;

  // If Privacy Hub is enabled, location ToS will no longer be tied to ARC and
  // instead will affect both ChromeOS and ARC.
  data.Set("isPrivacyHubLocationEnabled",
           ash::features::IsCrosPrivacyHubLocationEnabled());
  // If ARC is enabled, show the ARC ToS and the related opt-ins.
  data.Set("isArcEnabled", arc::IsArcTermsOfServiceOobeNegotiationNeeded());
  // In demo mode, don't show any opt-ins related to ARC and allow showing the
  // offline ARC ToS if the online version failed to load.
  data.Set("isDemo", arc::IsArcDemoModeSetupFlow());
  // Child accounts have alternative strings for the opt-ins.
  data.Set("isChildAccount", is_child_account_);
  // If the user is affiliated with the device management domain, ToS should be
  // hidden.
  data.Set("isTosHidden", enterprise_util::IsProfileAffiliated(profile));

  // ToS URLs.
  data.Set("googleEulaUrl", GetTosHost(ToS::GOOGLE_EULA));
  data.Set("crosEulaUrl", GetTosHost(ToS::CROS_EULA));
  data.Set("arcTosUrl", GetTosHost(ToS::ARC));
  data.Set("privacyPolicyUrl", GetTosHost(ToS::PRIVACY_POLICY));

  // Option that controls if Recovery factor opt-in should be shown for the
  // user.
  data.Set("showRecoveryOption",
           context()->recovery_setup.ask_about_recovery_consent);
  // Default value for recovery opt toggle.
  data.Set("recoveryOptionDefault",
           context()->recovery_setup.recovery_factor_opted_in);

  view_->Show(std::move(data));

  if (context()->extra_factors_token) {
    session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
        context()->extra_factors_token.value());
  }
}

void ConsolidatedConsentScreen::HideImpl() {
  pref_handler_.reset();
  session_refresher_.reset();
}

void ConsolidatedConsentScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kBackDemoButtonClicked) {
    exit_callback_.Run(Result::BACK_DEMO);
  } else if (action_id == kAcceptButtonClicked) {
    CHECK_EQ(args.size(), 6u);
    const bool enable_usage = args[1].GetBool();
    const bool enable_backup = args[2].GetBool();
    const bool enable_location = args[3].GetBool();
    const std::string& tos_content = args[4].GetString();
    const bool enable_recovery = args[5].GetBool();
    OnAccept(enable_usage, enable_backup, enable_location, tos_content,
             enable_recovery);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void ConsolidatedConsentScreen::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConsolidatedConsentScreen::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConsolidatedConsentScreen::OnMetricsModeChanged(bool enabled,
                                                     bool managed) {
  UpdateMetricsMode(enabled, managed);
}

void ConsolidatedConsentScreen::OnBackupAndRestoreModeChanged(bool enabled,
                                                              bool managed) {
  backup_restore_managed_ = managed;
  if (view_) {
    view_->SetBackupMode(enabled, managed);
  }
}

void ConsolidatedConsentScreen::OnLocationServicesModeChanged(bool enabled,
                                                              bool managed) {
  location_services_managed_ = managed;
  if (view_)
    view_->SetLocationMode(enabled, managed);
}

void ConsolidatedConsentScreen::UpdateMetricsMode(bool enabled, bool managed) {
  // When the usage opt-in is not managed, override the enabled value
  // with `true` to encourage users to consent with it during OptIn flow.
  if (view_) {
    view_->SetUsageMode(/*enabled=*/!managed || enabled, managed);
  }
}

void ConsolidatedConsentScreen::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  // If no ownership is established yet, then the current user is the first
  // user to sign in. Therefore, the current user would be the owner if the
  // device is not enterprise managed.
  bool is_managed = ash::InstallAttributes::Get()->IsEnterpriseManaged();

  if (status == DeviceSettingsService::OwnershipStatus::kOwnershipNone) {
    is_owner_ = !is_managed;
  } else if (status ==
             DeviceSettingsService::OwnershipStatus::kOwnershipTaken) {
    is_owner_ = user_manager::UserManager::Get()->IsCurrentUserOwner();
  }

  // Save this value for future reuse in the wizard flow. Note: it might remain
  // unset.
  context()->is_owner_flow = is_owner_;

  const bool is_demo = arc::IsArcDemoModeSetupFlow();

  // TODO(b/325492473): Evaluate logic to remove/simplify SetUsageOptInHidden.
  if (view_) {
    // All new accounts are shown the default usage opt-in.
    // Managed devices have their consent set by the device enterprise policy.
    // Unmanaged devices have UMA opted in by default with the ability to
    // toggle. view_->SetUsageMode() controls setting the value of the users
    // consent and controls whether the user is able to toggle the consent.
    //
    // If the consent screen is shown during other managed device flows,
    // the ability of the users to toggle the usage opt-in will be disabled
    // while still showing the value set by the enterprise.
    // Unmanaged user flows enable the option for the user to toggle.
    view_->SetUsageOptinHidden(false);
  }

  const bool is_negotiation_needed =
      ash::features::IsCrosPrivacyHubLocationEnabled()
          ? true
          : arc::IsArcTermsOfServiceOobeNegotiationNeeded();

  if (!is_demo && is_negotiation_needed) {
    // Enable ARC to match ArcSessionManager logic. ArcSessionManager expects
    // that ARC is enabled (prefs::kArcEnabled = true) on showing Terms of
    // Service. If user accepts ToS then prefs::kArcEnabled is left activated.
    // If user skips ToS then prefs::kArcEnabled is automatically reset in
    // ArcSessionManager.
    Profile* profile = ProfileManager::GetActiveUserProfile();
    DCHECK(profile);

    if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
      arc::SetArcPlayStoreEnabledForProfile(profile, true);
    }

    pref_handler_ = std::make_unique<arc::ArcOptInPreferenceHandler>(
        this, profile->GetPrefs(), g_browser_process->metrics_service());
    pref_handler_->Start();
  } else if (!is_demo) {
    // Since ARC OOBE Negotiation is not needed, we should avoid using
    // ArcOptInPreferenceHandler, so, we should update the usage opt-in here
    // since OnMetricsModeChanged() will not be called.
    auto* metrics_service = g_browser_process->metrics_service();
    bool is_enabled = false;
    if (metrics_service &&
        metrics_service->GetCurrentUserMetricsConsent().has_value()) {
      is_enabled = *metrics_service->GetCurrentUserMetricsConsent();
    } else {
      DCHECK(g_browser_process->local_state());
      is_enabled = StatsReportingController::Get()->IsEnabled();
    }

    UpdateMetricsMode(is_enabled, is_managed);
  }

  // Demo devices are not enterprise enrolled until after the consolidated
  // consent screen is shown, even though they're considered managed devices.
  // If in demo mode enrollment, show the UMA opt in toggle, but disable the
  // ability to toggle it since it's value is set by the enterprise policy.
  if (is_demo) {
    UpdateMetricsMode(/*enabled=*/true, /*managed=*/true);
  }
}

void ConsolidatedConsentScreen::RecordConsents(
    const ConsentsParameters& params) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // The account may or may not have consented to browser sync.
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  const CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::GIVEN);
  play_consent.set_confirmation_grd_id(
      IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);
  if (params.record_arc_tos_consent) {
    play_consent.set_confirmation_grd_id(
        IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
    play_consent.set_play_terms_of_service_text_length(
        params.tos_content.length());
    play_consent.set_play_terms_of_service_hash(
        base::SHA1HashString(params.tos_content));
  }
  consent_auditor->RecordArcPlayConsent(account_id, play_consent);

  if (params.record_backup_consent) {
    ArcBackupAndRestoreConsent backup_and_restore_consent;
    backup_and_restore_consent.set_confirmation_grd_id(
        IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
    backup_and_restore_consent.add_description_grd_ids(
        IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_TITLE);
    backup_and_restore_consent.add_description_grd_ids(
        is_child_account_ ? IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_CHILD
                          : IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN);
    backup_and_restore_consent.set_status(params.backup_accepted
                                              ? UserConsentTypes::GIVEN
                                              : UserConsentTypes::NOT_GIVEN);

    consent_auditor->RecordArcBackupAndRestoreConsent(
        account_id, backup_and_restore_consent);
  }

  if (params.record_location_consent) {
    ArcGoogleLocationServiceConsent location_service_consent;
    location_service_consent.set_confirmation_grd_id(
        IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);

    if (features::IsCrosPrivacyHubLocationEnabled()) {
      location_service_consent.add_description_grd_ids(
          IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_TITLE);
      location_service_consent.add_description_grd_ids(
          is_child_account_
              ? IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_CHILD
              : IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN);
    } else {
      location_service_consent.add_description_grd_ids(
          IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_TITLE);
      location_service_consent.add_description_grd_ids(
          is_child_account_ ? IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_CHILD
                            : IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN);
    }

    location_service_consent.set_status(params.location_accepted
                                            ? UserConsentTypes::GIVEN
                                            : UserConsentTypes::NOT_GIVEN);
    consent_auditor->RecordArcGoogleLocationServiceConsent(
        account_id, location_service_consent);
  }
}

void ConsolidatedConsentScreen::ReportUsageOptIn(bool is_enabled) {
  DCHECK(is_owner_.has_value());
  if (is_owner_.value()) {
    StatsReportingController::Get()->SetEnabled(
        ProfileManager::GetActiveUserProfile(), is_enabled);
    return;
  }

  auto* metrics_service = g_browser_process->metrics_service();
  DCHECK(metrics_service);

  // If user is not eligible for per-user, this will no-op. See details at
  // chrome/browser/metrics/per_user_state_manager_chromeos.h.
  metrics_service->UpdateCurrentUserMetricsConsent(is_enabled);
}

void ConsolidatedConsentScreen::NotifyConsolidatedConsentAcceptForTesting() {
  for (auto& observer : observer_list_) {
    observer.OnConsolidatedConsentAccept();
  }
}

void ConsolidatedConsentScreen::OnAccept(bool enable_stats_usage,
                                         bool enable_backup_restore,
                                         bool enable_location_services,
                                         const std::string& tos_content,
                                         bool enable_recovery) {
  ReportUsageOptIn(enable_stats_usage);

  context()->recovery_setup.recovery_factor_opted_in = enable_recovery;

  if (arc::IsArcDemoModeSetupFlow() ||
      !arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    for (auto& observer : observer_list_) {
      observer.OnConsolidatedConsentAccept();
    }

    ExitScreenWithAcceptedResult();
    return;
  }

  pref_handler_->EnableBackupRestore(enable_backup_restore);
  pref_handler_->EnableLocationService(enable_location_services);

  ConsentsParameters consents;
  consents.tos_content = tos_content;

  // If the profile is affiliated, we don't show any ToS to the user.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  consents.record_arc_tos_consent =
      !enterprise_util::IsProfileAffiliated(profile) && !tos_content.empty();
  consents.record_backup_consent = !backup_restore_managed_;
  consents.backup_accepted = enable_backup_restore;
  consents.record_location_consent = !location_services_managed_;
  consents.location_accepted = enable_location_services;
  RecordConsents(consents);

  for (auto& observer : observer_list_)
    observer.OnConsolidatedConsentAccept();

  ExitScreenWithAcceptedResult();
}

void ConsolidatedConsentScreen::ExitScreenWithAcceptedResult() {
  if (!chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    // Recovery is not set up for ephemeral users, don't send metrics in this
    // case.
    RecordRecoveryOptinResult(context()->recovery_setup);
  }
  StartupUtils::MarkEulaAccepted();
  network_portal_detector::GetInstance()->Enable();

  const DemoSetupController* const demo_setup_controller =
      WizardController::default_controller()->demo_setup_controller();

  if (!demo_setup_controller) {
    if (switches::IsRevenBranding()) {
      PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
      prefs->SetBoolean(prefs::kRevenOobeConsolidatedConsentAccepted, true);
    }
    exit_callback_.Run(Result::ACCEPTED);
    return;
  }
  exit_callback_.Run(Result::ACCEPTED_DEMO_ONLINE);
}

}  // namespace ash
