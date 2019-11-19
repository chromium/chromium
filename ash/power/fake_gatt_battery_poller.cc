// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/fake_gatt_battery_poller.h"

namespace ash {

FakeGattBatteryPoller::FakeGattBatteryPoller(
    const std::string& device_address,
    base::OnceClosure destructor_callback)
    : GattBatteryPoller(device_address),
      destructor_callback_(std::move(destructor_callback)) {}

FakeGattBatteryPoller::~FakeGattBatteryPoller() {
  std::move(destructor_callback_).Run();
}

}  // namespace ash
