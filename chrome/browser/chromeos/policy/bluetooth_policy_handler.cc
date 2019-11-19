// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/bluetooth_policy_handler.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace policy {

BluetoothPolicyHandler::BluetoothPolicyHandler(
    chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings) {
  bluetooth_policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kAllowBluetooth,
      base::Bind(&BluetoothPolicyHandler::OnBluetoothPolicyChanged,
                 weak_factory_.GetWeakPtr()));

  // Fire it once so we're sure we get an invocation on startup.
  OnBluetoothPolicyChanged();
}

BluetoothPolicyHandler::~BluetoothPolicyHandler() {}

void BluetoothPolicyHandler::OnBluetoothPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&BluetoothPolicyHandler::OnBluetoothPolicyChanged,
                     weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  device::BluetoothAdapterFactory::GetAdapter(base::BindOnce(
      &BluetoothPolicyHandler::SetBluetoothPolicy, weak_factory_.GetWeakPtr()));
}

void BluetoothPolicyHandler::SetBluetoothPolicy(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  // Get the updated policy.
  bool allow_bluetooth = true;
  cros_settings_->GetBoolean(chromeos::kAllowBluetooth, &allow_bluetooth);

  if (!allow_bluetooth) {
    adapter_ = adapter;
    adapter_->Shutdown();
  }
}

}  // namespace policy
