// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_sounds_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/test_system_sounds_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

using power_manager::PowerSupplyProperties;

namespace ash {

class PowerSoundsControllerTest : public AshTestBase {
 public:
  PowerSoundsControllerTest() = default;

  PowerSoundsControllerTest(const PowerSoundsControllerTest&) = delete;
  PowerSoundsControllerTest& operator=(const PowerSoundsControllerTest&) =
      delete;

  ~PowerSoundsControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_.InitAndEnableFeature(features::kSystemSounds);
    AshTestBase::SetUp();
  }

  TestSystemSoundsDelegate* GetSystemSoundsDelegate() const {
    return static_cast<TestSystemSoundsDelegate*>(
        Shell::Get()->system_sounds_delegate());
  }

  bool VerifySounds(const std::vector<Sound>& expected_sounds) const {
    const auto& actual_sounds =
        GetSystemSoundsDelegate()->last_played_sound_keys();

    if (actual_sounds.size() != expected_sounds.size())
      return false;

    for (size_t i = 0; i < expected_sounds.size(); ++i) {
      if (expected_sounds[i] != actual_sounds[i])
        return false;
    }

    return true;
  }

  void SetPowerStatus(int battery_level, bool line_power_connected) {
    PowerSupplyProperties proto;
    proto.set_external_power(
        line_power_connected
            ? power_manager::PowerSupplyProperties_ExternalPower_AC
            : power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
    proto.set_battery_percent(battery_level);
    proto.set_battery_time_to_empty_sec(3 * 60 * 60);
    proto.set_battery_time_to_full_sec(2 * 60 * 60);
    proto.set_is_calculating_battery_time(false);

    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
  }

  void SetInitialPowerStatus() { SetPowerStatus(5, false); }

 private:
  base::test::ScopedFeatureList scoped_feature_;
};

// Tests if sounds are played correctedly when the device is plugged at three
// different battery ranges.
TEST_F(PowerSoundsControllerTest, PlaySoundsForCharging) {
  SetInitialPowerStatus();
  // Expect no sounds at initial status, which has battery level at 5%, no line
  // power connected, and no charging.
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery level is in low range, from 0% to 15%, the sound for
  // plugging in should be `Sound::kChargeLowBattery`.
  SetPowerStatus(5, true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));

  // We should reset the sound key if the sound played successfully each time
  // for not affecting the next sound key.
  GetSystemSoundsDelegate()->reset();

  // Unplug the line power when battery level reaches out to 50%.
  SetPowerStatus(50, false);

  // When the battery level is in medium range, from 16% to 79%, the sound for
  // plugging in should be `Sound::kChargeMediumBattery`.
  SetPowerStatus(50, true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeMediumBattery}));
  GetSystemSoundsDelegate()->reset();

  // Unplug the line power when battery level reaches out to 90%.
  SetPowerStatus(90, false);

  // When the battery level is in high range, from 80% to 100%, the sound for
  // plugging in should be `Sound::kChargeHighBattery`.
  SetPowerStatus(90, true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeHighBattery}));
}

// Tests if the warning sound can be played when the battery level drops below
// 15% at the first time.
TEST_F(PowerSoundsControllerTest, PlaySoundForLowBatteryWarning) {
  // Don't play warning sound if the battery level is no less than 15%.
  SetPowerStatus(16, false);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery drops below 15% at the first time, e.g. 15%, the device
  // should play the sound for warning.
  SetPowerStatus(15, false);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  // When the battery level keeps dropping, the device shouldn't play sound for
  // warning.
  SetPowerStatus(14, false);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());
}

// Tests a very edge case we charge the device at the same time the warning
// sound is triggered.
TEST_F(PowerSoundsControllerTest, PlayTwoSoundsSimultaneously) {
  // Don't play warning sound if the battery level is no less than 15%.
  SetPowerStatus(16, false);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // Charge the device at the moment when the low battery warning sound is
  // played. The device should play two sounds.
  SetPowerStatus(15, true);
  EXPECT_TRUE(
      VerifySounds({Sound::kChargeLowBattery, Sound::kNoChargeLowBattery}));
}

}  // namespace ash