// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"

namespace arc {

ArcOptInPreferenceHandler::ArcOptInPreferenceHandler(
    ArcOptInPreferenceHandlerObserver* observer,
    PrefService* pref_service,
    metrics::MetricsService* metrics_service)
    : observer_(observer),
      pref_service_(pref_service),
      metrics_service_(metrics_service) {
  DCHECK(observer_);
  DCHECK(pref_service_);
  DCHECK(metrics_service_);
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
  OnMetricsPreferenceChanged();
  SendBackupAndRestoreMode();
  SendLocationServicesMode();
}

ArcOptInPreferenceHandler::~ArcOptInPreferenceHandler() = default;

void ArcOptInPreferenceHandler::OnMetricsPreferenceChanged() {
  auto* const device_settings_service = ash::DeviceSettingsService::Get();
  DCHECK(device_settings_service);

  device_settings_service->GetOwnershipStatusAsync(
      base::IgnoreArgs<ash::DeviceSettingsService::OwnershipStatus>(
          base::BindOnce(&ArcOptInPreferenceHandler::SendMetricsMode,
                         weak_ptr_factory_.GetWeakPtr())));
}

void ArcOptInPreferenceHandler::OnBackupAndRestorePreferenceChanged() {
  SendBackupAndRestoreMode();
}

void ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged() {
  SendLocationServicesMode();
}

void ArcOptInPreferenceHandler::EnableMetricsOnOwnershipKnown(
    bool metrics_enabled) {
  if (ShouldUpdateUserConsent()) {
    EnableUserMetrics(metrics_enabled);
  } else {
    // Handles case in which device is either not owned or per-user is not
    // enabled.
    ash::StatsReportingController::Get()->SetEnabled(
        ProfileManager::GetActiveUserProfile(), metrics_enabled);
  }

  DCHECK(enable_metrics_callback_);
  std::move(enable_metrics_callback_).Run();
}

void ArcOptInPreferenceHandler::SendMetricsMode() {
  if (ShouldUpdateUserConsent()) {
    observer_->OnMetricsModeChanged(GetUserMetrics(),
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
  bool enabled =
      pref_service_->HasPrefPath(prefs::kArcLocationServiceEnabled)
          ? pref_service_->GetBoolean(prefs::kArcLocationServiceEnabled)
          : true;

  // We should use device location setting during optin, in case user has
  // disabled location of device we should show the same preference during
  // opt-in. Default value of kUserGeolocationAccessLevel is
  // `AccessLevel::kAllowed`.
  if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub) &&
      // TODO(vsomani): Remove managed user check once
      // kUserGeolocationAccessLevel is in sync with the managed policy of
      // arcgooglelocationservicesenabled.
      !pref_service_->IsManagedPreference(prefs::kArcLocationServiceEnabled)) {
    enabled = ash::PrivacyHubController::CrosToArcGeolocationPermissionMapping(
        static_cast<ash::GeolocationAccessLevel>(pref_service_->GetInteger(
            ash::prefs::kUserGeolocationAccessLevel)));
  }
  observer_->OnLocationServicesModeChanged(
      enabled,
      pref_service_->IsManagedPreference(prefs::kArcLocationServiceEnabled));
}

void ArcOptInPreferenceHandler::EnableMetrics(bool is_enabled,
                                              base::OnceClosure callback) {
  auto* device_settings_service = ash::DeviceSettingsService::Get();
  DCHECK(device_settings_service);

  device_settings_service->GetOwnershipStatusAsync(
      base::IgnoreArgs<ash::DeviceSettingsService::OwnershipStatus>(
          base::BindOnce(
              &ArcOptInPreferenceHandler::EnableMetricsOnOwnershipKnown,
              weak_ptr_factory_.GetWeakPtr(), is_enabled)));

  enable_metrics_callback_ = std::move(callback);
}

void ArcOptInPreferenceHandler::EnableBackupRestore(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcBackupRestoreEnabled, is_enabled);
}

void ArcOptInPreferenceHandler::EnableLocationService(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcLocationServiceEnabled, is_enabled);
  if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub)) {
    pref_service_->SetBoolean(prefs::kArcInitialLocationSettingSyncRequired,
                              false);
    pref_service_->SetInteger(
        ash::prefs::kUserGeolocationAccessLevel,
        static_cast<int>(
            ash::PrivacyHubController::ArcToCrosGeolocationPermissionMapping(
                is_enabled)));
  }
}

bool ArcOptInPreferenceHandler::ShouldUpdateUserConsent() {
  // Return user consent should not be used if feature is disabled.
  if (!base::FeatureList::IsEnabled(ash::features::kPerUserMetrics)) {
    return false;
  }

  if (!metrics_service_->GetCurrentUserMetricsConsent().has_value()) {
    return false;
  }

  // Per user metrics should be disabled if the device metrics was disabled by
  // the owner.
  return ash::StatsReportingController::Get()->IsEnabled();
}

void ArcOptInPreferenceHandler::EnableUserMetrics(bool is_enabled) {
  // If user is not eligible for per-user, this will no-op. See details at
  // chrome/browser/metrics/per_user_state_manager_chromeos.h.
  metrics_service_->UpdateCurrentUserMetricsConsent(is_enabled);
}

bool ArcOptInPreferenceHandler::GetUserMetrics() {
  absl::optional<bool> metrics_enabled =
      metrics_service_->GetCurrentUserMetricsConsent();

  // No value means user is not eligible for per-user consent. This should be
  // caught by ShouldUpdateUserConsent().
  DCHECK(metrics_enabled.has_value());

  return *metrics_enabled;
}

}  // namespace arc
