// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_FAKE_GATT_BATTERY_PERCENTAGE_FETCHER_H_
#define ASH_POWER_FAKE_GATT_BATTERY_PERCENTAGE_FETCHER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/power/gatt_battery_percentage_fetcher.h"
#include "base/macros.h"

namespace ash {

// Fake implementation of GattBatteryPercentageFetcher to use in tests.
class ASH_EXPORT FakeGattBatteryPercentageFetcher
    : public GattBatteryPercentageFetcher {
 public:
  FakeGattBatteryPercentageFetcher(const std::string& device_address,
                                   BatteryPercentageCallback callback);

  ~FakeGattBatteryPercentageFetcher() override;

  using GattBatteryPercentageFetcher::InvokeCallbackWithFailedFetch;
  using GattBatteryPercentageFetcher::InvokeCallbackWithSuccessfulFetch;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGattBatteryPercentageFetcher);
};

}  // namespace ash

#endif  // ASH_POWER_FAKE_GATT_BATTERY_PERCENTAGE_FETCHER_H_
