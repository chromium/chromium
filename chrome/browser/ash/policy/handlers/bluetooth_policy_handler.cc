// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/bluetooth_policy_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace policy {

BluetoothPolicyHandler::BluetoothPolicyHandler(ash::CrosSettings* cros_settings)
    : cros_settings_(cros_settings) {
  allow_bluetooth_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kAllowBluetooth,
      base::BindRepeating(&BluetoothPolicyHandler::OnBluetoothPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  allowed_services_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceAllowedBluetoothServices,
      base::BindRepeating(&BluetoothPolicyHandler::OnBluetoothPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BluetoothPolicyHandler::InitializeOnAdapterReady,
                     weak_factory_.GetWeakPtr()));

  // Fire it once so we're sure we get an invocation on startup.
  OnBluetoothPolicyChanged();
}

BluetoothPolicyHandler::~BluetoothPolicyHandler() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void BluetoothPolicyHandler::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  SetBluetoothPolicy();
}

void BluetoothPolicyHandler::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  SetBluetoothPolicy();
}

void BluetoothPolicyHandler::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = std::move(adapter);
  adapter_->AddObserver(this);

  SetBluetoothPolicy();
}

void BluetoothPolicyHandler::OnBluetoothPolicyChanged() {
  ash::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&BluetoothPolicyHandler::OnBluetoothPolicyChanged,
                         weak_factory_.GetWeakPtr()));
  if (status != ash::CrosSettingsProvider::TRUSTED)
    return;

  new_policy_update_pending_ = true;
  SetBluetoothPolicy();
}

void SetServiceAllowListSuccess() {
  SYSLOG(INFO) << "Set ServiceAllowList Success";
}

void SetServiceAllowListFailed() {
  SYSLOG(ERROR) << "Set ServiceAllowList Failed";
}

void BluetoothPolicyHandler::SetBluetoothPolicy() {
  // Get the updated policy.
  bool allow_bluetooth = true;
  const base::Value::List* allowed_services_list = nullptr;
  std::vector<device::BluetoothUUID> allowed_services;

  if (!adapter_ || !adapter_->IsInitialized() || !adapter_->IsPresent() ||
      !adapter_->IsPowered() || !new_policy_update_pending_) {
    return;
  }

  cros_settings_->GetBoolean(ash::kAllowBluetooth, &allow_bluetooth);
  if (!allow_bluetooth) {
    adapter_->SetPowered(false, base::DoNothing(), base::DoNothing());
    adapter_->Shutdown();
  }

  // Pass empty list to SetServiceAllowList means no restriction for users,
  // which is the same behavior as leaving DeviceAllowedBluetoothService unset.
  // Therefore, we don't need to handle the case when device management server
  // returns an empty list even if the policy did not set.
  if (cros_settings_->GetList(ash::kDeviceAllowedBluetoothServices,
                              &allowed_services_list)) {
    for (const auto& list_value : *allowed_services_list) {
      if (!list_value.is_string())
        continue;

      const std::string& uuid_str = list_value.GetString();
      device::BluetoothUUID uuid(uuid_str);
      if (!uuid.IsValid()) {
        SYSLOG(WARNING) << "Failed to parse '" << uuid_str
                        << "' into UUID struct";
        continue;
      }

      allowed_services.push_back(uuid);
    }

    std::ostringstream info_stream;
    info_stream << "Setting " << allowed_services.size()
                << " UUIDs as allowed services;";
    for (size_t i = 0; i < allowed_services.size(); i++) {
      info_stream << " " << i << "th allowed service uuid: "
                  << allowed_services[i].canonical_value() << ";";
    }
    SYSLOG(INFO) << info_stream.str();

    adapter_->SetServiceAllowList(allowed_services,
                                  base::BindOnce(SetServiceAllowListSuccess),
                                  base::BindOnce(SetServiceAllowListFailed));
  }

  // Reset the indicator to false to make sure we don't bother setting the same
  // policy to the daemon, although it is harmless.
  new_policy_update_pending_ = false;
}

}  // namespace policy
