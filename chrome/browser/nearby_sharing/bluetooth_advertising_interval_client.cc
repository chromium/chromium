// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/bluetooth_advertising_interval_client.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace {

constexpr base::TimeDelta kIntervalMin = base::Milliseconds(100);
constexpr base::TimeDelta kIntervalMax = base::Milliseconds(100);

// A value of 0 will restore the interval to the system default.
constexpr base::TimeDelta kDefaultIntervalMin = base::Milliseconds(0);
constexpr base::TimeDelta kDefaultIntervalMax = base::Milliseconds(0);

}  // namespace

BluetoothAdvertisingIntervalClient::BluetoothAdvertisingIntervalClient(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {}

BluetoothAdvertisingIntervalClient::~BluetoothAdvertisingIntervalClient() {
  RestoreDefaultInterval();
}

void BluetoothAdvertisingIntervalClient::ReduceInterval() {
  adapter_->SetAdvertisingInterval(
      kIntervalMin, kIntervalMax, base::DoNothing(),
      base::BindOnce(
          &BluetoothAdvertisingIntervalClient::OnSetIntervalForAdvertisingError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdvertisingIntervalClient::RestoreDefaultInterval() {
  adapter_->SetAdvertisingInterval(
      kDefaultIntervalMin, kDefaultIntervalMax, base::DoNothing(),
      base::BindOnce(
          &BluetoothAdvertisingIntervalClient::OnRestoreDefaultIntervalError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdvertisingIntervalClient::OnSetIntervalForAdvertisingError(
    device::BluetoothAdvertisement::ErrorCode code) {
  CD_LOG(WARNING, Feature::NS)
      << __func__
      << ": SetAdvertisingInterval() failed with error code = " << code;
}

void BluetoothAdvertisingIntervalClient::OnRestoreDefaultIntervalError(
    device::BluetoothAdvertisement::ErrorCode code) {
  CD_LOG(WARNING, Feature::NS)
      << __func__
      << ": SetAdvertisingInterval() failed with error code = " << code;
}
