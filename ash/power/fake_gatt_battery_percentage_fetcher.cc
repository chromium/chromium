// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/fake_gatt_battery_percentage_fetcher.h"

namespace ash {

FakeGattBatteryPercentageFetcher::FakeGattBatteryPercentageFetcher(
    const std::string& device_address,
    BatteryPercentageCallback callback)
    : GattBatteryPercentageFetcher(device_address, std::move(callback)) {}

FakeGattBatteryPercentageFetcher::~FakeGattBatteryPercentageFetcher() = default;

}  // namespace ash
