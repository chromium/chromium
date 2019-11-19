// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_FAKE_GATT_BATTERY_POLLER_H_
#define ASH_POWER_FAKE_GATT_BATTERY_POLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/power/gatt_battery_poller.h"
#include "base/callback.h"
#include "base/macros.h"

namespace ash {

// Fake implementation of a GattBatteryPoller to use in tests.
class ASH_EXPORT FakeGattBatteryPoller : public GattBatteryPoller {
 public:
  FakeGattBatteryPoller(const std::string& device_address,
                        base::OnceClosure destructor_callback);
  ~FakeGattBatteryPoller() override;

 private:
  base::OnceClosure destructor_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeGattBatteryPoller);
};

}  // namespace ash

#endif  // ASH_POWER_FAKE_GATT_BATTERY_POLLER_H_
