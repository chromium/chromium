// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_
#define ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {
class ColorProvider;
}

namespace ash {

class ASH_EXPORT StylusBatteryDelegate
    : public PeripheralBatteryListener::Observer {
 public:
  using Callback = base::RepeatingCallback<void()>;

  StylusBatteryDelegate();
  StylusBatteryDelegate(const StylusBatteryDelegate& other) = delete;
  StylusBatteryDelegate& operator=(const StylusBatteryDelegate& other) = delete;
  ~StylusBatteryDelegate() override;

  SkColor GetColorForBatteryLevel() const;
  gfx::ImageSkia GetBatteryImage(const ui::ColorProvider* color_provider) const;
  gfx::ImageSkia GetBatteryStatusUnknownImage() const;
  void SetBatteryUpdateCallback(Callback battery_update_callback);
  bool IsBatteryCharging() const;
  bool IsBatteryLevelLow() const;
  bool IsBatteryStatusStale() const;
  bool IsBatteryStatusEligible() const;
  bool ShouldShowBatteryStatus() const;

  std::optional<uint8_t> battery_level() const { return battery_level_; }

 private:
  bool IsBatteryInfoValid(
      const PeripheralBatteryListener::BatteryInfo& battery) const;

  // PeripheralBatteryListener::Observer:
  void OnAddingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnRemovingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnUpdatedBatteryLevel(
      const PeripheralBatteryListener::BatteryInfo& battery) override;

  PeripheralBatteryListener::BatteryInfo::ChargeStatus battery_charge_status_ =
      PeripheralBatteryListener::BatteryInfo::ChargeStatus::kUnknown;
  std::optional<uint8_t> battery_level_;
  std::optional<base::TimeTicks> last_update_timestamp_;
  bool last_update_eligible_ = false;

  Callback battery_update_callback_;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      battery_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_STYLUS_BATTERY_DELEGATE_H_
