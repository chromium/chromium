// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"

namespace {

bool ShouldUpdateUserConsent() {
  // Return user consent should not be used if feature is disabled.
  if (!base::FeatureList::IsEnabled(ash::features::kPerUserMetrics))
    return false;

  auto* metrics_service = g_browser_process->metrics_service();

  if (!metrics_service ||
      !metrics_service->GetCurrentUserMetricsConsent().has_value()) {
    return false;
  }

  // Per user metrics should be disabled if the device metrics was disabled by
  // the owner.
  return ash::StatsReportingController::Get()->IsEnabled();
}

}  // namespace

namespace arc {

ArcOptInPreferenceHandler::ArcOptInPreferenceHandler(
    ArcOptInPreferenceHandlerObserver* observer,
    PrefService* pref_service)
    : observer_(observer), pref_service_(pref_service) {
  DCHECK(observer_);
  DCHECK(pref_service_);
}

void ArcOptInPreferenceHandler::Start() {
  reporting_consent_subscription_ =
      ash::StatsReportingController::Get()->AddObserver(base::BindRepeating(
          &ArcOptInPreferenceHandler::OnMetricsPreferenceChanged,
          base::Unretained(this)));

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kArcBackupRestoreEnabled,
      base::BindRepeating(
          &ArcOptInPreferenceHandler::OnBackupAndRestorePreferenceChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kArcLocationServiceEnabled,
      base::BindRepeating(
          &ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged,
          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(ash::features::kPerUserMetrics)) {
    pref_change_registrar_.Add(
        metrics::prefs::kMetricsUserConsent,
        base::BindRepeating(
            &ArcOptInPreferenceHandler::OnMetricsPreferenceChanged,
            base::Unretained(this)));
  }

  // Send current state.
  SendMetricsMode();
  SendBackupAndRestoreMode();
  SendLocationServicesMode();
}

ArcOptInPreferenceHandler::~ArcOptInPreferenceHandler() {}

void ArcOptInPreferenceHandler::OnMetricsPreferenceChanged() {
  SendMetricsMode();
}

void ArcOptInPreferenceHandler::OnBackupAndRestorePreferenceChanged() {
  SendBackupAndRestoreMode();
}

void ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged() {
  SendLocationServicesMode();
}

void ArcOptInPreferenceHandler::SendMetricsMode() {
  if (ShouldUpdateUserConsent()) {
    auto* metrics_service = g_browser_process->metrics_service();
    DCHECK(metrics_service);

    absl::optional<bool> metrics_enabled =
        g_browser_process->metrics_service()->GetCurrentUserMetricsConsent();

    // No value means user is not eligible for per-user consent. This should be
    // caught by ShouldUpdateUserConsent().
    DCHECK(metrics_enabled.has_value());

    observer_->OnMetricsModeChanged(*metrics_enabled,
                                    IsMetricsReportingPolicyManaged());
  } else if (g_browser_process->local_state()) {
    bool enabled = ash::StatsReportingController::Get()->IsEnabled();
    observer_->OnMetricsModeChanged(enabled, IsMetricsReportingPolicyManaged());
  }
}

void ArcOptInPreferenceHandler::SendBackupAndRestoreMode() {
  // Override the pref default to the true value, in order to encourage users to
  // consent with it during OptIn flow.
  const bool enabled =
      pref_service_->HasPrefPath(prefs::kArcBackupRestoreEnabled)
          ? pref_service_->GetBoolean(prefs::kArcBackupRestoreEnabled)
          : true;
  observer_->OnBackupAndRestoreModeChanged(
      enabled,
      pref_service_->IsManagedPreference(prefs::kArcBackupRestoreEnabled));
}

void ArcOptInPreferenceHandler::SendLocationServicesMode() {
  // Override the pref default to the true value, in order to encourage users to
  // consent with it during OptIn flow.
  const bool enabled =
      pref_service_->HasPrefPath(prefs::kArcLocationServiceEnabled)
          ? pref_service_->GetBoolean(prefs::kArcLocationServiceEnabled)
          : true;
  observer_->OnLocationServicesModeChanged(
      enabled,
      pref_service_->IsManagedPreference(prefs::kArcLocationServiceEnabled));
}

void ArcOptInPreferenceHandler::EnableMetrics(bool is_enabled) {
  if (ShouldUpdateUserConsent()) {
    auto* metrics_service = g_browser_process->metrics_service();
    DCHECK(metrics_service);

    // If user is not eligible for per-user, this will no-op. See details at
    // chrome/browser/metrics/per_user_state_manager_chromeos.h.
    metrics_service->UpdateCurrentUserMetricsConsent(is_enabled);
    return;
  }

  // Handles case in which device is either not owned or per-user is not
  // enabled.
  ash::StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), is_enabled);
}

void ArcOptInPreferenceHandler::EnableBackupRestore(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcBackupRestoreEnabled, is_enabled);
}

void ArcOptInPreferenceHandler::EnableLocationService(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcLocationServiceEnabled, is_enabled);
}

}  // namespace arc
