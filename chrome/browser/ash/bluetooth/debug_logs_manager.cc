// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace bluetooth {

namespace {
const char kVerboseLoggingEnablePrefName[] = "bluetooth.verboseLogging.enable";

const uint8_t kVerboseDisabledLevel = 0;
const uint8_t kVerboseBasicLevel = 1;

const int kDbusRetryCount = 10;
constexpr base::TimeDelta kDbusRetryInterval = base::TimeDelta::FromSeconds(3);
}  // namespace

DebugLogsManager::DebugLogsManager(const std::string& primary_user_email,
                                   PrefService* pref_service)
    : primary_user_email_(primary_user_email), pref_service_(pref_service) {
  // For Googlers, set the default preference of Bluetooth verbose logs to true.
  if (AreDebugLogsSupported() &&
      !pref_service->HasPrefPath(kVerboseLoggingEnablePrefName)) {
    ChangeDebugLogsState(true);
  }

  SetVerboseLogsEnable(GetDebugLogsState() ==
                       DebugLogsState::kSupportedAndEnabled);
}

DebugLogsManager::~DebugLogsManager() {
  SetVerboseLogsEnable(false);
}

// static
void DebugLogsManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kVerboseLoggingEnablePrefName,
                                false /* default_value */);
}

DebugLogsManager::DebugLogsState DebugLogsManager::GetDebugLogsState() const {
  if (!AreDebugLogsSupported())
    return DebugLogsState::kNotSupported;

  return pref_service_->GetBoolean(kVerboseLoggingEnablePrefName)
             ? DebugLogsState::kSupportedAndEnabled
             : DebugLogsState::kSupportedButDisabled;
}

mojo::PendingRemote<mojom::DebugLogsChangeHandler>
DebugLogsManager::GenerateRemote() {
  mojo::PendingRemote<mojom::DebugLogsChangeHandler> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void DebugLogsManager::ChangeDebugLogsState(bool should_debug_logs_be_enabled) {
  DCHECK_NE(GetDebugLogsState(), DebugLogsState::kNotSupported);

  pref_service_->SetBoolean(kVerboseLoggingEnablePrefName,
                            should_debug_logs_be_enabled);
}

bool DebugLogsManager::AreDebugLogsSupported() const {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kShowBluetoothDebugLogToggle)) {
    return false;
  }

  return gaia::IsGoogleInternalAccountEmail(primary_user_email_);
}

void DebugLogsManager::SetVerboseLogsEnable(bool enable) {
  SendDBusVerboseLogsMessage(enable, 0 /* num_completed_attempts */);
}

void DebugLogsManager::SendDBusVerboseLogsMessage(bool enable,
                                                  int num_completed_attempts) {
  uint8_t level = enable ? kVerboseBasicLevel : kVerboseDisabledLevel;
  VLOG(1) << (enable ? "Enabling" : "Disabling") << " bluetooth verbose logs";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothDebugManagerClient()
      ->SetLogLevels(
          0 /* dispatcher */, 0 /* newblue */, level /* bluez */,
          0 /* kernel */,
          base::BindOnce(&DebugLogsManager::OnVerboseLogsEnableSuccess,
                         weak_ptr_factory_.GetWeakPtr(), enable),
          base::BindOnce(&DebugLogsManager::OnVerboseLogsEnableError,
                         weak_ptr_factory_.GetWeakPtr(), enable,
                         num_completed_attempts));
}

void DebugLogsManager::OnVerboseLogsEnableSuccess(bool enable) {
  VLOG(1) << "Bluetooth verbose logs successfully "
          << (enable ? "enabled" : "disabled");
}

void DebugLogsManager::OnVerboseLogsEnableError(
    const bool enable,
    const int num_completed_attempts,
    const std::string& error_name,
    const std::string& error_message) {
  bool should_retry = (num_completed_attempts < kDbusRetryCount);

  LOG(ERROR) << "Setting bluetooth verbose logs failed: error: " << error_name
             << " - " << error_message << " "
             << (should_retry ? "Retrying." : "Giving up.");

  if (!should_retry)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DebugLogsManager::SendDBusVerboseLogsMessage,
                     weak_ptr_factory_.GetWeakPtr(), enable,
                     num_completed_attempts + 1),
      kDbusRetryInterval);
}

}  // namespace bluetooth

}  // namespace ash
