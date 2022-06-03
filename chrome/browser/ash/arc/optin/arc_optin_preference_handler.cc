// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"

#include "base/bind.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"

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
  if (g_browser_process->local_state()) {
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
