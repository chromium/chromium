// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"

#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace policy {

namespace {

constexpr base::TimeDelta kAdbSideloadingPlannedNotificationWaitTime =
    base::Days(1);

std::optional<AdbSideloadingAllowanceMode> GetAdbSideloadingDevicePolicyMode(
    const ash::CrosSettings* cros_settings,
    const base::RepeatingClosure callback) {
  auto status = cros_settings->PrepareTrustedValues(callback);

  // If the policy value is still not trusted, return optional null
  if (status != ash::CrosSettingsProvider::TRUSTED) {
    return std::nullopt;
  }

  // Get the trusted policy value.
  int sideloading_mode = -1;
  if (!cros_settings->GetInteger(ash::kDeviceCrostiniArcAdbSideloadingAllowed,
                                 &sideloading_mode)) {
    // Here we do not return null because we want to handle this case separately
    // and to reset all the prefs for the notifications so that they can be
    // displayed again if the policy changes
    return AdbSideloadingAllowanceMode::kNotSet;
  }

  using Mode =
      enterprise_management::DeviceCrostiniArcAdbSideloadingAllowedProto;

  switch (sideloading_mode) {
    case Mode::DISALLOW:
      return AdbSideloadingAllowanceMode::kDisallow;
    case Mode::DISALLOW_WITH_POWERWASH:
      return AdbSideloadingAllowanceMode::kDisallowWithPowerwash;
    case Mode::ALLOW_FOR_AFFILIATED_USERS:
      return AdbSideloadingAllowanceMode::kAllowForAffiliatedUser;
    default:
      return std::nullopt;
  }
}

}  // namespace

// static
void AdbSideloadingAllowanceModePolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kForceFactoryReset, false);
  registry->RegisterBooleanPref(
      prefs::kAdbSideloadingDisallowedNotificationShown, false);
  registry->RegisterTimePref(
      prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime,
      base::Time::Min());
  registry->RegisterBooleanPref(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown, false);
}

AdbSideloadingAllowanceModePolicyHandler::
    AdbSideloadingAllowanceModePolicyHandler(
        ash::CrosSettings* cros_settings,
        PrefService* local_state,
        chromeos::PowerManagerClient* power_manager_client,
        ash::AdbSideloadingPolicyChangeNotification*
            adb_sideloading_policy_change_notification)
    : cros_settings_(cros_settings),
      local_state_(local_state),
      adb_sideloading_policy_change_notification_(
          adb_sideloading_policy_change_notification),
      power_manager_observer_(this) {
  DCHECK(local_state_);
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceCrostiniArcAdbSideloadingAllowed,
      base::BindRepeating(
          &AdbSideloadingAllowanceModePolicyHandler::MaybeShowNotification,
          weak_factory_.GetWeakPtr()));

  check_sideloading_status_callback_ = base::BindRepeating(
      &AdbSideloadingAllowanceModePolicyHandler::CheckSideloadingStatus,
      weak_factory_.GetWeakPtr());

  notification_timer_ = std::make_unique<base::OneShotTimer>();

  DCHECK(power_manager_client);
  power_manager_observer_.Observe(power_manager_client);
}

AdbSideloadingAllowanceModePolicyHandler::
    ~AdbSideloadingAllowanceModePolicyHandler() = default;

void AdbSideloadingAllowanceModePolicyHandler::
    SetCheckSideloadingStatusCallbackForTesting(
        const CheckSideloadingStatusCallback& callback) {
  check_sideloading_status_callback_ = callback;
}

void AdbSideloadingAllowanceModePolicyHandler::SetNotificationTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  notification_timer_ = std::move(timer);
}

void AdbSideloadingAllowanceModePolicyHandler::MaybeShowNotification() {
  std::optional<AdbSideloadingAllowanceMode> mode =
      GetAdbSideloadingDevicePolicyMode(
          cros_settings_,
          base::BindRepeating(
              &AdbSideloadingAllowanceModePolicyHandler::MaybeShowNotification,
              weak_factory_.GetWeakPtr()));

  if (!mode.has_value()) {
    return;
  }

  switch (*mode) {
    case AdbSideloadingAllowanceMode::kDisallow:
      // Reset the prefs for the powerwash notifications so they can be shown
      // again if the policy changes
      ClearPowerwashNotificationPrefs();
      check_sideloading_status_callback_.Run(
          base::BindOnce(&AdbSideloadingAllowanceModePolicyHandler::
                             MaybeShowDisallowedNotification,
                         weak_factory_.GetWeakPtr()));
      return;
    case AdbSideloadingAllowanceMode::kDisallowWithPowerwash:
      // Reset the pref for the disallowed notification so it can be shown again
      // if the policy changes
      ClearDisallowedNotificationPref();
      check_sideloading_status_callback_.Run(
          base::BindOnce(&AdbSideloadingAllowanceModePolicyHandler::
                             MaybeShowPowerwashNotification,
                         weak_factory_.GetWeakPtr()));
      return;
    case AdbSideloadingAllowanceMode::kNotSet:
    case AdbSideloadingAllowanceMode::kAllowForAffiliatedUser:
      // Reset all the prefs so the notifications can be shown again if the
      // policy changes
      ClearDisallowedNotificationPref();
      ClearPowerwashNotificationPrefs();
      notification_timer_->Stop();
      return;
  }
}

void AdbSideloadingAllowanceModePolicyHandler::CheckSideloadingStatus(
    base::OnceCallback<void(bool)> callback) {
  // If the feature is not enabled, never show a notification
  if (!base::FeatureList::IsEnabled(
          ash::features::kArcManagedAdbSideloadingSupport)) {
    std::move(callback).Run(false);
    return;
  }

  using ResponseCode = ash::SessionManagerClient::AdbSideloadResponseCode;

  auto* client = ash::SessionManagerClient::Get();
  client->QueryAdbSideload(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback, ResponseCode response_code,
         bool enabled) {
        switch (response_code) {
          case ResponseCode::SUCCESS:
            // Everything is fine, so pass the |enabled| value to |callback|.
            break;
          case ResponseCode::FAILED:
            // Status could not be established so return false so that the
            // notifications would not be shown.
            enabled = false;
            break;
          case ResponseCode::NEED_POWERWASH:
            // This can only happen on devices initialized before M74, i.e. not
            // powerwashed since then. Do not show the notifications there.
            enabled = false;
            break;
        }
        std::move(callback).Run(enabled);
      },
      std::move(callback)));
}

void AdbSideloadingAllowanceModePolicyHandler::
    ShowAdbSideloadingPolicyChangeNotificationIfNeeded() {
  MaybeShowNotification();
}

bool AdbSideloadingAllowanceModePolicyHandler::
    WasDisallowedNotificationShown() {
  return local_state_->GetBoolean(
      prefs::kAdbSideloadingDisallowedNotificationShown);
}

bool AdbSideloadingAllowanceModePolicyHandler::
    WasPowerwashOnNextRebootNotificationShown() {
  return local_state_->GetBoolean(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown);
}

void AdbSideloadingAllowanceModePolicyHandler::MaybeShowDisallowedNotification(
    bool is_sideloading_enabled) {
  if (!is_sideloading_enabled)
    return;

  if (WasDisallowedNotificationShown())
    return;

  local_state_->SetBoolean(prefs::kAdbSideloadingDisallowedNotificationShown,
                           true);
  adb_sideloading_policy_change_notification_->Show(
      NotificationType::kSideloadingDisallowed);
}

void AdbSideloadingAllowanceModePolicyHandler::MaybeShowPowerwashNotification(
    bool is_sideloading_enabled) {
  if (!is_sideloading_enabled)
    return;

  base::Time notification_shown_time = local_state_->GetTime(
      prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime);

  // If the time has not been set yet, set it and show the planned notification
  if (notification_shown_time.is_min()) {
    notification_shown_time = base::Time::Now();
    local_state_->SetTime(
        prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime,
        notification_shown_time);
    adb_sideloading_policy_change_notification_->Show(
        NotificationType::kPowerwashPlanned);
  }

  // Show update on next reboot notification after at least
  // |kAdbSideloadingPlannedNotificationWaitTime|.
  base::Time show_reboot_notification_time =
      notification_shown_time + kAdbSideloadingPlannedNotificationWaitTime;

  // If this time has already been reached stop the timer and show the
  // notification immediately.
  if (show_reboot_notification_time <= base::Time::Now()) {
    notification_timer_->Stop();
    MaybeShowPowerwashUponRebootNotification();
    return;
  }

  // Otherwise set a timer that will display the |kPowerwashOnNextReboot|
  // notification no earlier than |kAdbSideloadingPlannedNotificationWaitTime|
  // after showing the |kPowerwashPlanned| notification.
  if (notification_timer_->IsRunning())
    return;
  notification_timer_->Start(
      FROM_HERE, show_reboot_notification_time - base::Time::Now(), this,
      &AdbSideloadingAllowanceModePolicyHandler::
          MaybeShowPowerwashUponRebootNotification);
}

void AdbSideloadingAllowanceModePolicyHandler::
    MaybeShowPowerwashUponRebootNotification() {
  if (WasPowerwashOnNextRebootNotificationShown())
    return;

  local_state_->SetBoolean(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown, true);

  // Set this right away to ensure the user is forced to powerwash on next
  // start even if they ignore the notification and do not click the button
  local_state_->SetBoolean(prefs::kForceFactoryReset, true);
  local_state_->CommitPendingWrite();

  adb_sideloading_policy_change_notification_->Show(
      NotificationType::kPowerwashOnNextReboot);
}

void AdbSideloadingAllowanceModePolicyHandler::
    ClearDisallowedNotificationPref() {
  local_state_->ClearPref(prefs::kAdbSideloadingDisallowedNotificationShown);
}

void AdbSideloadingAllowanceModePolicyHandler::
    ClearPowerwashNotificationPrefs() {
  local_state_->ClearPref(
      prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime);
  local_state_->ClearPref(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown);
}

void AdbSideloadingAllowanceModePolicyHandler::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  // Try showing the notification when the screen wakes up from idle state
  if (!state.off()) {
    MaybeShowNotification();
  }
}

void AdbSideloadingAllowanceModePolicyHandler::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  // Try showing the notification when the user opens the lid
  if (state == chromeos::PowerManagerClient::LidState::OPEN) {
    MaybeShowNotification();
  }
}

}  // namespace policy
