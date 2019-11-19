// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_HID_BATTERY_LISTENER_H_
#define ASH_POWER_HID_BATTERY_LISTENER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {

// Listens to changes in battery level for HID devices, updating the
// corresponding device::BluetoothDevice.
class ASH_EXPORT HidBatteryListener
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit HidBatteryListener(scoped_refptr<device::BluetoothAdapter> adapter);
  ~HidBatteryListener() override;

 private:
  friend class HidBatteryListenerTest;

  // chromeos::PowerManagerClient::Observer:
  void PeripheralBatteryStatusReceived(const std::string& path,
                                       const std::string& name,
                                       int level) override;

  scoped_refptr<device::BluetoothAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(HidBatteryListener);
};

}  // namespace ash

#endif  // ASH_POWER_HID_BATTERY_LISTENER_H_
