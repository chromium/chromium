// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_BLUETOOTH_ADVERTISING_INTERVAL_CLIENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_BLUETOOTH_ADVERTISING_INTERVAL_CLIENT_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

// BluetoothAdvertisingIntervalClient is responsible for setting the Bluetooth
// advertising interval to a lower value when we start advertising for Nearby
// Share, and then restores the interval to the system default when advertising
// stops.
class BluetoothAdvertisingIntervalClient {
 public:
  BluetoothAdvertisingIntervalClient(
      scoped_refptr<device::BluetoothAdapter> adapter);
  ~BluetoothAdvertisingIntervalClient();

  // Sets the advertising interval to a lowered value to allow for faster
  // connections.
  void ReduceInterval();
  // Restores the advertising interval to the system default.
  void RestoreDefaultInterval();

 private:
  void OnSetIntervalForAdvertisingError(
      device::BluetoothAdvertisement::ErrorCode code);
  void OnRestoreDefaultIntervalError(
      device::BluetoothAdvertisement::ErrorCode code);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::WeakPtrFactory<BluetoothAdvertisingIntervalClient> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_BLUETOOTH_ADVERTISING_INTERVAL_CLIENT_H_
