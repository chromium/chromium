// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/device_disabling_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace system {

DeviceDisablingManager::Observer::~Observer() {
}

DeviceDisablingManager::Delegate::~Delegate() {
}

DeviceDisablingManager::DeviceDisablingManager(
    Delegate* delegate,
    CrosSettings* cros_settings,
    user_manager::UserManager* user_manager)
    : delegate_(delegate),
      browser_policy_connector_(g_browser_process->platform_part()
                                    ->browser_policy_connector_chromeos()),
      cros_settings_(cros_settings),
      user_manager_(user_manager),
      device_disabled_(false) {
  CHECK(delegate_);
}

DeviceDisablingManager::~DeviceDisablingManager() {
}

void DeviceDisablingManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceDisablingManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceDisablingManager::Init() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDeviceDisabling)) {
    // If device disabling is turned off by flags, do not start monitoring cros
    // settings.
    return;
  }

  device_disabled_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceDisabled,
      base::Bind(&DeviceDisablingManager::UpdateFromCrosSettings,
                 weak_factory_.GetWeakPtr()));
  disabled_message_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceDisabledMessage,
      base::Bind(&DeviceDisablingManager::UpdateFromCrosSettings,
                 weak_factory_.GetWeakPtr()));

  UpdateFromCrosSettings();
}

void DeviceDisablingManager::CacheDisabledMessageAndNotify(
    const std::string& disabled_message) {
  if (disabled_message == disabled_message_)
    return;

  disabled_message_ = disabled_message;
  for (auto& observer : observers_)
    observer.OnDisabledMessageChanged(disabled_message_);
}

void DeviceDisablingManager::CheckWhetherDeviceDisabledDuringOOBE(
    const DeviceDisabledCheckCallback& callback) {
  if (policy::GetDeviceStateMode() != policy::RESTORE_MODE_DISABLED ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDeviceDisabling)) {
    // Indicate that the device is not disabled if it is not marked as such in
    // local state or device disabling has been turned off by flag.
    callback.Run(false);
    return;
  }

  if (browser_policy_connector_->GetDeviceMode() ==
          policy::DEVICE_MODE_PENDING) {
    // If the device mode is not known yet, request to be called back once it
    // becomes known.
    browser_policy_connector_->GetInstallAttributes()->ReadImmutableAttributes(
        base::Bind(
            &DeviceDisablingManager::CheckWhetherDeviceDisabledDuringOOBE,
            weak_factory_.GetWeakPtr(),
            callback));
    return;
  }

  if (browser_policy_connector_->GetDeviceMode() !=
          policy::DEVICE_MODE_NOT_SET) {
    // If the device is owned already, this method must have been called after
    // OOBE, which is an error. Indicate that the device is not disabled to
    // prevent spurious disabling. Actual device disabling after OOBE will be
    // handled elsewhere, by checking for disabled state in cros settings.
    LOG(ERROR) << "CheckWhetherDeviceDisabledDuringOOBE() called after OOBE.";
    callback.Run(false);
    return;
  }

  // The device is marked as disabled in local state (based on the device state
  // retrieved early during OOBE). Since device disabling has not been turned
  // off by flag and the device is still unowned, we honor the information in
  // local state and consider the device disabled.

  // Update the enrollment domain.
  enrollment_domain_.clear();
  g_browser_process->local_state()->GetDictionary(
      prefs::kServerBackedDeviceState)->GetString(
          policy::kDeviceStateManagementDomain,
          &enrollment_domain_);

  // Update the serial number.
  serial_number_ = chromeos::system::StatisticsProvider::GetInstance()
                       ->GetEnterpriseMachineID();

  // Update the disabled message.
  std::string disabled_message;
  g_browser_process->local_state()->GetDictionary(
      prefs::kServerBackedDeviceState)->GetString(
          policy::kDeviceStateDisabledMessage,
          &disabled_message);
  CacheDisabledMessageAndNotify(disabled_message);

  // Indicate that the device is disabled.
  callback.Run(true);
}

// static
bool DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation() {
  bool device_disabled = false;
  CrosSettings::Get()->GetBoolean(kDeviceDisabled, &device_disabled);
  if (device_disabled && HonorDeviceDisablingDuringNormalOperation()) {
    return true;
  }

  return false;
}

// static
bool DeviceDisablingManager::HonorDeviceDisablingDuringNormalOperation() {
  // Device disabling should be honored when the device is enterprise managed
  // and device disabling has not been turned off by flag.
  return g_browser_process->platform_part()
             ->browser_policy_connector_chromeos()
             ->IsEnterpriseManaged() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableDeviceDisabling);
}

void DeviceDisablingManager::UpdateFromCrosSettings() {
  if (cros_settings_->PrepareTrustedValues(base::Bind(
          &DeviceDisablingManager::UpdateFromCrosSettings,
          weak_factory_.GetWeakPtr())) != CrosSettingsProvider::TRUSTED) {
    // If the cros settings are not trusted yet, request to be called back
    // later.
    return;
  }

  if (!HonorDeviceDisablingDuringNormalOperation()) {
    // If the device is not enterprise managed or device disabling has been
    // turned of by flag, device disabling is not available.
    return;
  }

  bool should_device_be_disabled = false;
  if (!cros_settings_->GetBoolean(kDeviceDisabled,
                                  &should_device_be_disabled) ||
      !should_device_be_disabled) {
    // The device should not be disabled.

    if (!device_disabled_) {
      // If the device is currently not disabled, there is nothing to do.
      return;
    }

    // Re-enable the device.
    device_disabled_ = false;

    // The device was disabled and has been re-enabled. Normal function should
    // be resumed. Since the device disabled screen abruptly interrupts the
    // regular login screen flows, Chrome should be restarted to return to a
    // well-defined state.
    delegate_->RestartToLoginScreen();
    return;
  }

  // Update the disabled message.
  std::string disabled_message;
  cros_settings_->GetString(kDeviceDisabledMessage, &disabled_message);
  CacheDisabledMessageAndNotify(disabled_message);

  if (device_disabled_) {
    // If the device was disabled already, updating the disabled message is the
    // only action required.
    return;
  }
  device_disabled_ = true;

  const ExistingUserController* existing_user_controller =
      ExistingUserController::current_controller();
  if (user_manager_->GetActiveUser() ||
      (existing_user_controller &&
       existing_user_controller->IsSigninInProgress())) {
    // If a session or a login is in progress, restart Chrome and return to the
    // login screen. Chrome will show the device disabled screen after the
    // restart.
    delegate_->RestartToLoginScreen();
    return;
  }

  // Cache the enrollment domain.
  enrollment_domain_ =
      browser_policy_connector_->GetEnterpriseEnrollmentDomain();

  // Cache the device serial number.
  serial_number_ = chromeos::system::StatisticsProvider::GetInstance()
                       ->GetEnterpriseMachineID();

  // If no session or login is in progress, show the device disabled screen.
  delegate_->ShowDeviceDisabledScreen();
}

}  // namespace system
}  // namespace chromeos
