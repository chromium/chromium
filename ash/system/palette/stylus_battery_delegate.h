// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_
#define ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "base/scoped_observation.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class ASH_EXPORT StylusBatteryDelegate
    : public PeripheralBatteryListener::Observer {
 public:
  StylusBatteryDelegate();
  StylusBatteryDelegate(const StylusBatteryDelegate& other) = delete;
  StylusBatteryDelegate& operator=(const StylusBatteryDelegate& other) = delete;
  ~StylusBatteryDelegate() override;

  SkColor GetColorForBatteryLevel() const;
  int GetLabelIdForBatteryLevel() const;
  gfx::ImageSkia GetBatteryImage() const;

  base::Optional<uint8_t> battery_level() const { return battery_level_; }

 private:
  // PeripheralBatteryListener::Observer:
  void OnAddingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnRemovingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnUpdatedBatteryLevel(
      const PeripheralBatteryListener::BatteryInfo& battery) override;

  base::Optional<uint8_t> battery_level_;

  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      battery_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_
