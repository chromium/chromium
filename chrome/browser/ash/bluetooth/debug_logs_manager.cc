// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/floss/floss_logging_client.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace bluetooth {

namespace {
const char kVerboseLoggingEnablePrefName[] = "bluetooth.verboseLogging.enable";

const uint8_t kVerboseDisabledLevel = 0;
const uint8_t kVerboseBasicLevel = 1;

const int kDbusRetryCount = 10;
constexpr base::TimeDelta kDbusRetryInterval = base::Seconds(3);
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

  SetBluetoothQualityReport(
      /*enable=*/features::IsBluetoothQualityReportEnabled(),
      /*num_completed_attempts=*/0);

  // Grab the Bluetooth adapter instance so we can register observers.
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&DebugLogsManager::OnBluetoothAdapterAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

DebugLogsManager::~DebugLogsManager() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_.reset();
  }

  SetVerboseLogsEnable(false);

  // Disable Bluetooth Quality Report if it is enabled on login.
  if (features::IsBluetoothQualityReportEnabled())
    SetBluetoothQualityReport(/*enable=*/false,
                              /*num_completed_attempts=*/0);
}

// static
void DebugLogsManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kVerboseLoggingEnablePrefName,
                                /*default_value=*/false);
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
  if (!base::FeatureList::IsEnabled(features::kShowBluetoothDebugLogToggle))
    return false;

  return gaia::IsGoogleInternalAccountEmail(primary_user_email_);
}

void DebugLogsManager::SetVerboseLogsEnable(bool enable) {
  debug_logs_enabled_ = enable;
  SendDBusVerboseLogsMessage(enable, /*num_completed_attempts=*/0);
}

void DebugLogsManager::SendDBusVerboseLogsMessage(bool enable,
                                                  int num_completed_attempts) {
  uint8_t level = enable ? kVerboseBasicLevel : kVerboseDisabledLevel;
  VLOG(1) << (enable ? "Enabling" : "Disabling") << " bluetooth verbose logs";

  if (floss::features::IsFlossEnabled()) {
    if (adapter_ && adapter_->IsPowered()) {
      floss::FlossDBusManager::Get()->GetLoggingClient()->SetDebugLogging(
          base::BindOnce(&DebugLogsManager::OnFlossSetDebugLogging,
                         weak_ptr_factory_.GetWeakPtr(), enable,
                         num_completed_attempts),
          enable);
    }
  } else {
    bluez::BluezDBusManager::Get()
        ->GetBluetoothDebugManagerClient()
        ->SetLogLevels(
            /*bluez_level=*/level, /*kernel_level=*/0,
            base::BindOnce(&DebugLogsManager::OnVerboseLogsEnableSuccess,
                           weak_ptr_factory_.GetWeakPtr(), enable),
            base::BindOnce(&DebugLogsManager::OnVerboseLogsEnableError,
                           weak_ptr_factory_.GetWeakPtr(), enable,
                           num_completed_attempts));
  }
}

void DebugLogsManager::OnFlossSetDebugLogging(
    const bool enable,
    const int num_completed_attempts,
    floss::DBusResult<floss::Void> result) {
  if (result.has_value()) {
    OnVerboseLogsEnableSuccess(enable);
  } else {
    auto error = result.error();
    OnVerboseLogsEnableError(enable, num_completed_attempts, error.name,
                             error.message);
  }
}

void DebugLogsManager::OnVerboseLogsEnableSuccess(const bool enable) {
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DebugLogsManager::SendDBusVerboseLogsMessage,
                     weak_ptr_factory_.GetWeakPtr(), enable,
                     num_completed_attempts + 1),
      kDbusRetryInterval);
}

void DebugLogsManager::SetBluetoothQualityReport(bool enable,
                                                 int num_completed_attempts) {
  VLOG(1) << (enable ? "Enabling" : "Disabling") << " Bluetooth Quality Report";

  if (floss::features::IsFlossEnabled()) {
    VLOG(1) << "Floss does not yet support Bluetooth Quality Report.";
    return;
  }

  bluez::BluezDBusManager::Get()
      ->GetBluetoothDebugManagerClient()
      ->SetBluetoothQualityReport(
          enable,
          base::BindOnce(&DebugLogsManager::OnSetBluetoothQualityReportSuccess,
                         weak_ptr_factory_.GetWeakPtr(), enable),
          base::BindOnce(&DebugLogsManager::OnSetBluetoothQualityReportError,
                         weak_ptr_factory_.GetWeakPtr(), enable,
                         num_completed_attempts));
}

void DebugLogsManager::OnSetBluetoothQualityReportSuccess(bool enable) {
  VLOG(1) << "Bluetooth Quality Report successfully "
          << (enable ? "enabled" : "disabled");
}

void DebugLogsManager::OnSetBluetoothQualityReportError(
    const bool enable,
    const int num_completed_attempts,
    const std::string& error_name,
    const std::string& error_message) {
  bool should_retry = (num_completed_attempts < kDbusRetryCount);

  LOG(ERROR) << "Setting Bluetooth Quality Report failed: error: " << error_name
             << " - " << error_message << " "
             << (should_retry ? "Retrying." : "Giving up.");

  if (!should_retry)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DebugLogsManager::SetBluetoothQualityReport,
                     weak_ptr_factory_.GetWeakPtr(), enable,
                     num_completed_attempts + 1),
      kDbusRetryInterval);
}

void DebugLogsManager::OnBluetoothAdapterAvailable(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = std::move(adapter);
  adapter_->AddObserver(this);
}

void DebugLogsManager::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                             bool powered) {
  // Bluez does not dynamically set log level (it is persistent).
  if (!floss::features::IsFlossEnabled()) {
    return;
  }

  // We only need to send enable call when powering on and we want to enable
  // debug logs. We don't need to turn it off on power on.
  if (powered && debug_logs_enabled_) {
    SendDBusVerboseLogsMessage(debug_logs_enabled_,
                               /*num_completed_attempts=*/0);
  }
}

}  // namespace bluetooth

}  // namespace ash
