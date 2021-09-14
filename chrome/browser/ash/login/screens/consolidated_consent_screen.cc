// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/i18n/timezone.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {
std::string GetEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kOnlineEulaURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

std::string GetAdditionalToSUrl() {
  return base::StringPrintf(chrome::kAdditionalToSOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

}  // namespace

std::string ConsolidatedConsentScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED:
    case Result::ACCEPTED_DEMO_ONLINE:
    case Result::ACCEPTED_DEMO_OFFLINE:
      return "Accepted";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

ConsolidatedConsentScreen::ConsolidatedConsentScreen(
    ConsolidatedConsentScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ConsolidatedConsentScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

ConsolidatedConsentScreen::~ConsolidatedConsentScreen() {
  if (view_) {
    view_->Unbind();
  }
}

void ConsolidatedConsentScreen::OnViewDestroyed(
    ConsolidatedConsentScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool ConsolidatedConsentScreen::MaybeSkip(WizardContext* context) {
  if (arc::IsArcDemoModeSetupFlow())
    return false;

  // For managed users, admins are required to accept ToS on the server side.
  // So, if the user is managed and no Negotiation is needed, skip the screen.
  bool is_managed_account = ProfileManager::GetActiveUserProfile()
                                ->GetProfilePolicyConnector()
                                ->IsManaged();
  if (is_managed_account && !arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void ConsolidatedConsentScreen::ShowImpl() {
  if (!view_)
    return;

  bool is_demo = arc::IsArcDemoModeSetupFlow();
  bool is_arc_enabled = arc::IsArcTermsOfServiceOobeNegotiationNeeded();
  if (!is_demo && is_arc_enabled) {
    is_child_account_ =
        user_manager::UserManager::Get()->IsLoggedInAsChildUser();

    Profile* profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);

    // Enable ARC to match ArcSessionManager logic. ArcSessionManager expects
    // that ARC is enabled (prefs::kArcEnabled = true) on showing Terms of
    // Service. If user accepts ToS then prefs::kArcEnabled is left activated.
    // If user skips ToS then prefs::kArcEnabled is automatically reset in
    // ArcSessionManager.
    arc::SetArcPlayStoreEnabledForProfile(profile, true);
    arc_managed_ =
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile);

    pref_handler_ = std::make_unique<arc::ArcOptInPreferenceHandler>(
        this, profile->GetPrefs());
    pref_handler_->Start();
  }

  ConsolidatedConsentScreenView::ScreenConfig config;
  config.is_arc_enabled = is_arc_enabled;
  config.is_demo = is_demo;
  config.is_arc_managed = arc_managed_;
  config.is_child_account = is_child_account_;
  config.country_code = base::CountryCodeForCurrentTimezone();
  config.eula_url = GetEulaOnlineUrl();
  config.additional_tos_url = GetAdditionalToSUrl();
  view_->Show(config);
}

void ConsolidatedConsentScreen::HideImpl() {
  pref_handler_.reset();
}

void ConsolidatedConsentScreen::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConsolidatedConsentScreen::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConsolidatedConsentScreen::OnMetricsModeChanged(bool enabled,
                                                     bool managed) {
  if (view_)
    view_->SetUsageMode(enabled, managed);
}

void ConsolidatedConsentScreen::OnBackupAndRestoreModeChanged(bool enabled,
                                                              bool managed) {
  backup_restore_managed_ = managed;
  if (view_)
    view_->SetBackupMode(enabled, managed);
}

void ConsolidatedConsentScreen::OnLocationServicesModeChanged(bool enabled,
                                                              bool managed) {
  location_services_managed_ = managed;
  if (view_)
    view_->SetLocationMode(enabled, managed);
}

void ConsolidatedConsentScreen::RecordConsents(
    const ConsentsParameters& params) {
  // TODO: Implement after strings added
}

void ConsolidatedConsentScreen::OnAccept(bool enable_stats_usage,
                                         bool enable_backup_restore,
                                         bool enable_location_services,
                                         const std::string& tos_content) {
  // TODO: Handle usage stats reporting for current user

  if (arc::IsArcDemoModeSetupFlow() ||
      !arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    for (auto& observer : observer_list_)
      observer.OnConsolidatedConsentAccept();

    ExitScreenWithAcceptedResult();
    return;
  }

  pref_handler_->EnableBackupRestore(enable_backup_restore);
  pref_handler_->EnableLocationService(enable_location_services);

  ConsentsParameters consents;
  consents.tos_content = tos_content;
  consents.record_arc_tos_consent = !arc_managed_;
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
  const DemoSetupController* const demo_setup_controller =
      WizardController::default_controller()->demo_setup_controller();

  if (!demo_setup_controller) {
    exit_callback_.Run(Result::ACCEPTED);
    return;
  }
  if (demo_setup_controller->IsOfflineEnrollment())
    exit_callback_.Run(Result::ACCEPTED_DEMO_OFFLINE);
  else
    exit_callback_.Run(Result::ACCEPTED_DEMO_ONLINE);
}
}  // namespace ash
