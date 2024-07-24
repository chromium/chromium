// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_ADAPTER_STATUS_CHANGE_OBSERVER_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_ADAPTER_STATUS_CHANGE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "device/fido/fido_request_handler_base.h"

// Observer for observing the bluetooth adapter's status.
class DigitalIdentityBluetoothAdapterStatusChangeObserver
    : public base::CheckedObserver {
 public:
  virtual void OnBluetoothAdapterStatusChanged(
      device::FidoRequestHandlerBase::BleStatus ble_status) = 0;

 protected:
  ~DigitalIdentityBluetoothAdapterStatusChangeObserver() override = default;
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_ADAPTER_STATUS_CHANGE_OBSERVER_H_
