// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/tpm_auto_update_mode_policy_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace {

// Timeout for the TPM firmware update availability check.
const base::TimeDelta kFirmwareAvailabilityCheckerTimeout =
    base::TimeDelta::FromSeconds(20);

const base::TimeDelta kTPMUpdatePlannedNotificationWaitTime =
    base::TimeDelta::FromDays(1);

// Reads the value of the the device setting key
// TPMFirmwareUpdateSettings.AutoUpdateMode from a trusted store. If the value
// is temporarily untrusted |callback| will be invoked later when trusted values
// are available and AutoUpdateMode::kNever will be returned. This value is set
// via the device policy TPMFirmwareUpdateSettings.
policy::AutoUpdateMode GetTPMAutoUpdateModeSetting(
    const chromeos::CrosSettings* cros_settings,
    const base::RepeatingClosure callback) {
  if (!g_browser_process->platform_part()
           ->browser_policy_connector_chromeos()
           ->IsEnterpriseManaged()) {
    return policy::AutoUpdateMode::kNever;
  }

  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(callback);
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return policy::AutoUpdateMode::kNever;
  const base::Value* tpm_settings =
      cros_settings->GetPref(chromeos::kTPMFirmwareUpdateSettings);

  if (!tpm_settings)
    return policy::AutoUpdateMode::kNever;

  const base::Value* const auto_update_mode = tpm_settings->FindKeyOfType(
      chromeos::tpm_firmware_update::kSettingsKeyAutoUpdateMode,
      base::Value::Type::INTEGER);

  // Policy not set.
  if (!auto_update_mode || auto_update_mode->GetInt() == 0)
    return policy::AutoUpdateMode::kNever;

  // Verify that the value is within range.
  if (auto_update_mode->GetInt() <
          static_cast<int>(policy::AutoUpdateMode::kNever) ||
      auto_update_mode->GetInt() >
          static_cast<int>(policy::AutoUpdateMode::kEnrollment)) {
    NOTREACHED() << "Invalid value for device policy key "
                    "TPMFirmwareUpdateSettings.AutoUpdateMode";
    return policy::AutoUpdateMode::kNever;
  }

  return static_cast<policy::AutoUpdateMode>(auto_update_mode->GetInt());
}

}  // namespace

namespace policy {

TPMAutoUpdateModePolicyHandler::TPMAutoUpdateModePolicyHandler(
    chromeos::CrosSettings* cros_settings,
    PrefService* local_state)
    : cros_settings_(cros_settings), local_state_(local_state) {
  DCHECK(local_state_);
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kTPMFirmwareUpdateSettings,
      base::BindRepeating(&TPMAutoUpdateModePolicyHandler::OnPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  update_checker_callback_ =
      base::BindRepeating(&TPMAutoUpdateModePolicyHandler::CheckForUpdate,
                          weak_factory_.GetWeakPtr());

  show_notification_callback_ =
      base::BindRepeating(&chromeos::ShowAutoUpdateNotification);

  notification_timer_ = std::make_unique<base::OneShotTimer>();

  // Fire it once so we're sure we get an invocation on startup.
  OnPolicyChanged();
}

TPMAutoUpdateModePolicyHandler::~TPMAutoUpdateModePolicyHandler() = default;

void TPMAutoUpdateModePolicyHandler::OnPolicyChanged() {
  AutoUpdateMode auto_update_mode = GetTPMAutoUpdateModeSetting(
      cros_settings_,
      base::BindRepeating(&TPMAutoUpdateModePolicyHandler::OnPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  if (auto_update_mode == AutoUpdateMode::kNever ||
      auto_update_mode == AutoUpdateMode::kEnrollment) {
    if (notification_timer_->IsRunning())
      notification_timer_->Stop();
    return;
  }

  if (auto_update_mode == AutoUpdateMode::kUserAcknowledgment) {
    if (!WasTPMUpdateOnNextRebootNotificationShown()) {
      update_checker_callback_.Run(base::BindOnce(
          &TPMAutoUpdateModePolicyHandler::ShowTPMAutoUpdateNotification,
          weak_factory_.GetWeakPtr()));
      return;
    }
  }
  update_checker_callback_.Run(base::BindOnce(&OnUpdateAvailableCheckResult));
}

void TPMAutoUpdateModePolicyHandler::CheckForUpdate(
    base::OnceCallback<void(bool)> callback) {
  chromeos::tpm_firmware_update::UpdateAvailable(
      std::move(callback), kFirmwareAvailabilityCheckerTimeout);
}

// static
void TPMAutoUpdateModePolicyHandler::OnUpdateAvailableCheckResult(
    bool update_available) {
  if (!update_available)
    return;

  chromeos::SessionManagerClient::Get()->StartTPMFirmwareUpdate(
      "preserve_stateful");
}

void TPMAutoUpdateModePolicyHandler::SetUpdateCheckerCallbackForTesting(
    const UpdateCheckerCallback& callback) {
  update_checker_callback_ = callback;
}

void TPMAutoUpdateModePolicyHandler::SetShowNotificationCallbackForTesting(
    const ShowNotificationCallback& callback) {
  show_notification_callback_ = callback;
}

void TPMAutoUpdateModePolicyHandler::UpdateOnEnrollmentIfNeeded() {
  AutoUpdateMode auto_update_mode = GetTPMAutoUpdateModeSetting(
      cros_settings_,
      base::BindRepeating(
          &TPMAutoUpdateModePolicyHandler::UpdateOnEnrollmentIfNeeded,
          weak_factory_.GetWeakPtr()));

  // If the TPM is set to update with user acknowlegment, always update on
  // enrollment. The AutoUpdateMode.UserAcknowledgment flow is meant to give a
  // warning to the user to backup their data. During enrollment there is no
  // user data on the device.
  if (auto_update_mode == AutoUpdateMode::kEnrollment ||
      auto_update_mode == AutoUpdateMode::kUserAcknowledgment) {
    update_checker_callback_.Run(base::BindOnce(&OnUpdateAvailableCheckResult));
  }
}

void TPMAutoUpdateModePolicyHandler::ShowTPMAutoUpdateNotificationIfNeeded() {
  AutoUpdateMode auto_update_mode = GetTPMAutoUpdateModeSetting(
      cros_settings_,
      base::BindRepeating(&TPMAutoUpdateModePolicyHandler::OnPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  if (auto_update_mode != AutoUpdateMode::kUserAcknowledgment)
    return;

  update_checker_callback_.Run(base::BindOnce(
      &TPMAutoUpdateModePolicyHandler::ShowTPMAutoUpdateNotification,
      weak_factory_.GetWeakPtr()));
}

void TPMAutoUpdateModePolicyHandler::ShowTPMAutoUpdateNotification(
    bool update_available) {
  if (!update_available)
    return;

  if (!user_manager::UserManager::IsInitialized())
    return;

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  if (!user_manager->IsUserLoggedIn() || user_manager->IsLoggedInAsKioskApp())
    return;

  base::Time notification_shown =
      local_state_->GetTime(prefs::kTPMUpdatePlannedNotificationShownTime);

  if (notification_shown == base::Time::Min()) {
    notification_shown = base::Time::Now();
    local_state_->SetTime(prefs::kTPMUpdatePlannedNotificationShownTime,
                          notification_shown);
    show_notification_callback_.Run(
        chromeos::TpmAutoUpdateUserNotification::kPlanned);
  }

  // Show update on next reboot notification after
  // |kTPMUpdatePlannedNotificationWaitTime|.
  base::Time show_reboot_notification_time =
      notification_shown + kTPMUpdatePlannedNotificationWaitTime;

  if (show_reboot_notification_time <= base::Time::Now()) {
    if (notification_timer_->IsRunning())
      notification_timer_->Stop();
    ShowTPMUpdateOnNextRebootNotification();
    return;
  }

  // Set a timer that will display the update on next reboot notification after
  // 24 hours.
  if (notification_timer_->IsRunning())
    return;
  notification_timer_->Start(
      FROM_HERE, show_reboot_notification_time - base::Time::Now(), this,
      &TPMAutoUpdateModePolicyHandler::ShowTPMUpdateOnNextRebootNotification);
}

bool TPMAutoUpdateModePolicyHandler::
    WasTPMUpdateOnNextRebootNotificationShown() {
  return local_state_->GetBoolean(
      prefs::kTPMUpdateOnNextRebootNotificationShown);
}

// static
void TPMAutoUpdateModePolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kTPMUpdatePlannedNotificationShownTime,
                             base::Time::Min());
  registry->RegisterBooleanPref(prefs::kTPMUpdateOnNextRebootNotificationShown,
                                false);
}

void TPMAutoUpdateModePolicyHandler::ShowTPMUpdateOnNextRebootNotification() {
  local_state_->SetBoolean(prefs::kTPMUpdateOnNextRebootNotificationShown,
                           true);

  show_notification_callback_.Run(
      chromeos::TpmAutoUpdateUserNotification::kOnNextReboot);
}

void TPMAutoUpdateModePolicyHandler::SetNotificationTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  notification_timer_ = std::move(timer);
}

}  // namespace policy
