// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
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
  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    // TODO(b/325438501): Migrate `kUserGeolocationAccessLevel` to
    // ChromeOS-specific preference handler.
    pref_change_registrar_.Add(
        ash::prefs::kUserGeolocationAccessLevel,
        base::BindRepeating(
            &ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged,
            base::Unretained(this)));
  } else {
    pref_change_registrar_.Add(
        prefs::kArcLocationServiceEnabled,
        base::BindRepeating(
            &ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged,
            base::Unretained(this)));
  }

  pref_change_registrar_.Add(
      metrics::prefs::kMetricsUserConsent,
      base::BindRepeating(
          &ArcOptInPreferenceHandler::OnMetricsPreferenceChanged,
          base::Unretained(this)));

  // Send current state.
  OnMetricsPreferenceChanged();
  SendBackupAndRestoreMode();
  SendLocationServicesMode();
}

ArcOptInPreferenceHandler::~ArcOptInPreferenceHandler() = default;

void ArcOptInPreferenceHandler::OnMetricsPreferenceChanged() {
  auto* const device_settings_service = ash::DeviceSettingsService::Get();
  DCHECK(device_settings_service);

  // Async callback guarantees device ownership status is known.
  device_settings_service->GetOwnershipStatusAsync(
      base::BindOnce(&ArcOptInPreferenceHandler::SendMetricsMode,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcOptInPreferenceHandler::OnBackupAndRestorePreferenceChanged() {
  SendBackupAndRestoreMode();
}

void ArcOptInPreferenceHandler::OnLocationServicePreferenceChanged() {
  SendLocationServicesMode();
}

void ArcOptInPreferenceHandler::EnableMetricsOnOwnershipKnown(
    bool metrics_enabled,
    ash::DeviceSettingsService::OwnershipStatus ownership_status) {
  if (IsAllowedToUpdateUserConsent(ownership_status)) {
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

void ArcOptInPreferenceHandler::SendMetricsMode(
    ash::DeviceSettingsService::OwnershipStatus ownership_status) {
  if (IsAllowedToUpdateUserConsent(ownership_status)) {
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
  bool enabled = false;
  bool managed = false;

  // We should use device location setting during optin, in case user has
  // disabled location of device we should show the same preference during
  // opt-in. Default value of kUserGeolocationAccessLevel is
  // `AccessLevel::kAllowed`.
  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    enabled = ash::PrivacyHubController::CrosToArcGeolocationPermissionMapping(
        static_cast<ash::GeolocationAccessLevel>(pref_service_->GetInteger(
            ash::prefs::kUserGeolocationAccessLevel)));
    managed = pref_service_->IsManagedPreference(
        ash::prefs::kUserGeolocationAccessLevel);
  } else {
    // Legacy handling.
    // Override the pref default to the true value, in order to encourage users
    // to consent with it during OptIn flow.
    enabled = pref_service_->HasPrefPath(prefs::kArcLocationServiceEnabled)
                  ? pref_service_->GetBoolean(prefs::kArcLocationServiceEnabled)
                  : true;
    managed =
        pref_service_->IsManagedPreference(prefs::kArcLocationServiceEnabled);
  }

  observer_->OnLocationServicesModeChanged(enabled, managed);
}

void ArcOptInPreferenceHandler::EnableMetrics(bool is_enabled,
                                              base::OnceClosure callback) {
  auto* device_settings_service = ash::DeviceSettingsService::Get();
  DCHECK(device_settings_service);

  device_settings_service->GetOwnershipStatusAsync(
      base::BindOnce(&ArcOptInPreferenceHandler::EnableMetricsOnOwnershipKnown,
                     weak_ptr_factory_.GetWeakPtr(), is_enabled));

  enable_metrics_callback_ = std::move(callback);
}

void ArcOptInPreferenceHandler::EnableBackupRestore(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcBackupRestoreEnabled, is_enabled);
}

void ArcOptInPreferenceHandler::EnableLocationService(bool is_enabled) {
  pref_service_->SetBoolean(prefs::kArcLocationServiceEnabled, is_enabled);
  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    pref_service_->SetBoolean(prefs::kArcInitialLocationSettingSyncRequired,
                              false);
    if (auto* controller = ash::GeolocationPrivacySwitchController::Get()) {
      controller->SetAccessLevel(
          is_enabled ? ash::GeolocationAccessLevel::kAllowed
                     : ash::GeolocationAccessLevel::kDisallowed);
    }
    // We can also set the value of GeoLocation Accuracy as currently they are
    // in sync with Geo location.
    pref_service_->SetBoolean(ash::prefs::kUserGeolocationAccuracyEnabled,
                              is_enabled);
  }
}

bool ArcOptInPreferenceHandler::IsAllowedToUpdateUserConsent(
    ash::DeviceSettingsService::OwnershipStatus ownership_status) {
  // Managed devices should not use per-user consent.
  // Devices that fail this check are unmanaged, referred to as
  // having consumer ownership.
  if (IsMetricsReportingPolicyManaged()) {
    return false;
  }

  // Unmanaged guest sessions can be started while ownership status is None.
  // Guest sessions use per-user consent, asked during the ToS in guest OOBE.
  if (ProfileManager::GetActiveUserProfile()->IsGuestSession()) {
    return true;
  }

  bool is_device_owner =
      ownership_status ==
      ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone;

  // If the ownership is none, we assume that this is the device owner.
  // Owner users do not use per-user consent, as owner consent is handled by
  // ash::StatsReportingController.
  if (is_device_owner) {
    return false;
  }

  bool is_per_user_consent_enabled =
      metrics_service_->GetCurrentUserMetricsConsent().has_value();

  // Check that per-user set the current user consent.
  // Per-user is only enabled on unmanaged devices with secondary users.
  if (!is_per_user_consent_enabled) {
    return false;
  }

  // Check if the current user is the device owner.
  bool is_owner_user =
      (ownership_status ==
       ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken) &&
      user_manager::UserManager::Get()->IsCurrentUserOwner();

  // As a precaution, check is_owner_user even though
  // !is_per_user_consent_enabled should correctly disable owner users.
  if (is_owner_user) {
    return false;
  }

  return true;
}

void ArcOptInPreferenceHandler::EnableUserMetrics(bool is_enabled) {
  // If user is not eligible for per-user, this will no-op. See details at
  // chrome/browser/metrics/per_user_state_manager_chromeos.h.
  metrics_service_->UpdateCurrentUserMetricsConsent(is_enabled);
}

bool ArcOptInPreferenceHandler::GetUserMetrics() {
  std::optional<bool> metrics_enabled =
      metrics_service_->GetCurrentUserMetricsConsent();

  // No value means user is not eligible for per-user consent. This should be
  // caught by IsAllowedToUpdateUserConsent().
  DCHECK(metrics_enabled.has_value());

  return *metrics_enabled;
}

}  // namespace arc
