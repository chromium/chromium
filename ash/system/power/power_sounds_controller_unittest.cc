// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_sounds_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/test_system_sounds_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

using power_manager::PowerSupplyProperties;

namespace ash {

namespace {

using ExternalPower = power_manager::PowerSupplyProperties_ExternalPower;
constexpr ExternalPower kAcPower =
    power_manager::PowerSupplyProperties_ExternalPower_AC;
constexpr ExternalPower kUsbPower =
    power_manager::PowerSupplyProperties_ExternalPower_USB;
constexpr ExternalPower kDisconnectedPower =
    power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;

constexpr int kCriticalPercentage = 5;
constexpr int kLowPowerPercentage = 10;
constexpr int kCriticalMinutes = 5;
constexpr int kLowPowerMinutes = 15;

}  // namespace

class PowerSoundsControllerTest : public AshTestBase {
 public:
  explicit PowerSoundsControllerTest(
      std::optional<bool> battery_saver_allowed = false)
      : battery_saver_allowed_(battery_saver_allowed) {}

  PowerSoundsControllerTest(const PowerSoundsControllerTest&) = delete;
  PowerSoundsControllerTest& operator=(const PowerSoundsControllerTest&) =
      delete;

  ~PowerSoundsControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_.InitWithFeatures({features::kBatterySaver}, {});
    AshTestBase::SetUp();
    OverrideIsBatterySaverAllowedForTesting(battery_saver_allowed_);
    SetInitialPowerStatus();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    OverrideIsBatterySaverAllowedForTesting(std::nullopt);
  }

  TestSystemSoundsDelegate* GetSystemSoundsDelegate() const {
    return static_cast<TestSystemSoundsDelegate*>(
        Shell::Get()->system_sounds_delegate());
  }

  bool VerifySounds(const std::vector<Sound>& expected_sounds) const {
    const auto& actual_sounds =
        GetSystemSoundsDelegate()->last_played_sound_keys();

    if (actual_sounds.size() != expected_sounds.size()) {
      return false;
    }

    for (size_t i = 0; i < expected_sounds.size(); ++i) {
      if (expected_sounds[i] != actual_sounds[i]) {
        return false;
      }
    }

    return true;
  }

  // Returns true if the lid is closed.
  bool SetLidState(bool closed) {
    chromeos::FakePowerManagerClient::Get()->SetLidState(
        closed ? chromeos::PowerManagerClient::LidState::CLOSED
               : chromeos::PowerManagerClient::LidState::OPEN,
        base::TimeTicks::Now());
    return Shell::Get()
               ->system_notification_controller()
               ->power_sounds_->lid_state_ ==
           chromeos::PowerManagerClient::LidState::CLOSED;
  }

  void SetPowerStatus(int battery_level,
                      ExternalPower external_power,
                      int minutes_to_empty = 180) {
    ASSERT_GE(battery_level, 0);
    ASSERT_LE(battery_level, 100);

    const bool old_ac_charger_connected = is_ac_charger_connected_;
    is_ac_charger_connected_ = external_power == kAcPower;

    PowerSupplyProperties proto;
    proto.set_external_power(external_power);
    proto.set_battery_percent(battery_level);
    proto.set_battery_time_to_empty_sec(minutes_to_empty * 60);
    proto.set_battery_time_to_full_sec(2 * 60 * 60);
    proto.set_is_calculating_battery_time(false);

    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);

    // Records the battery level only when it's a plugin or unplug event.
    if (old_ac_charger_connected != is_ac_charger_connected_) {
      if (is_ac_charger_connected_) {
        plugged_in_levels_samples_[battery_level]++;
      } else {
        unplugged_levels_samples_[battery_level]++;
      }
    }
  }

  void SetInitialPowerStatus() {
    // The two toggle buttons are disabled as default, to test features, we will
    // initialize it as enabled.
    local_state()->SetBoolean(prefs::kChargingSoundsEnabled, true);
    local_state()->SetBoolean(prefs::kLowBatterySoundEnabled, true);

    // The default status for power is connected with a charger and the battery
    // level is 1%. We set the initial power status for each unit test to
    // disconnected with a charger and 5% battery level.
    is_ac_charger_connected_ = true;
    SetPowerStatus(5, kDisconnectedPower);
    EXPECT_FALSE(SetLidState(/*closed=*/false));
  }

 protected:
  base::HistogramTester histogram_tester_;

  base::flat_map</*battery_level=*/int, /*sample_count=*/int>
      plugged_in_levels_samples_;
  base::flat_map</*battery_level=*/int, /*sample_count=*/int>
      unplugged_levels_samples_;
  base::test::ScopedFeatureList scoped_feature_;

 private:
  bool is_ac_charger_connected_;
  std::optional<bool> battery_saver_allowed_;
};

class PowerSoundsControllerWithBatterySaverTest
    : public PowerSoundsControllerTest,
      public testing::WithParamInterface<
          features::BatterySaverNotificationBehavior> {
 public:
  PowerSoundsControllerWithBatterySaverTest()
      : PowerSoundsControllerTest(true) {}
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PowerSoundsControllerWithBatterySaverTest,
    testing::Values(features::BatterySaverNotificationBehavior::kBSMAutoEnable,
                    features::BatterySaverNotificationBehavior::kBSMOptIn));

TEST_P(PowerSoundsControllerWithBatterySaverTest,
       PlayLowBatterySoundForBatterySaver) {
  // Don't play warning sound if the battery level is no less than the low power
  // threshold for battery saver.
  const int battery_saver_threshold =
      features::kBatterySaverActivationChargePercent.Get();
  GetSystemSoundsDelegate()->reset();
  SetPowerStatus(battery_saver_threshold + 1, kDisconnectedPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery drops below the low power threshold at the first time, the
  // device should play the sound for warning.
  SetPowerStatus(battery_saver_threshold, kDisconnectedPower);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  // When the battery level keeps dropping but no less than the critical power
  // threshold, the device shouldn't play sound for warning.
  SetPowerStatus(battery_saver_threshold - 1, kDisconnectedPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // The device will play the sound if its battery level keeps dropping to the
  // critical power threshold.
  SetPowerStatus(kCriticalPercentage, kDisconnectedPower);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
}

// Tests if sounds are played correctedly when the device is plugged at three
// different battery ranges with a AC charger.
TEST_F(PowerSoundsControllerTest, PlaySoundsForCharging) {
  // Expect no sounds at the initial status when a device has a battery level of
  // 5%.
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery level is in low range, from 0% to 15%, the sound for
  // plugging in should be `Sound::kChargeLowBattery`.
  SetPowerStatus(5, kAcPower);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));

  // We should reset the sound key if the sound played successfully each time
  // for not affecting the next sound key.
  GetSystemSoundsDelegate()->reset();

  // Unplug the ac charger when battery level reaches out to 50%.
  SetPowerStatus(50, kDisconnectedPower);

  // When the battery level is in medium range, from 16% to 79%, the sound for
  // plugging in should be `Sound::kChargeMediumBattery`.
  SetPowerStatus(50, kAcPower);
  EXPECT_TRUE(VerifySounds({Sound::kChargeMediumBattery}));
  GetSystemSoundsDelegate()->reset();

  // Unplug the ac charger when battery level reaches out to 90%.
  SetPowerStatus(90, kDisconnectedPower);

  // When the battery level is in high range, from 80% to 100%, the sound for
  // plugging in should be `Sound::kChargeHighBattery`.
  SetPowerStatus(90, kAcPower);
  EXPECT_TRUE(VerifySounds({Sound::kChargeHighBattery}));
  GetSystemSoundsDelegate()->reset();

  SetPowerStatus(95, kDisconnectedPower);

  // Verifies no charging sound will be played if the device is connected with a
  // USB charger(low-power charger).
  SetPowerStatus(95, kUsbPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());
}

// Tests that when the user disables the toggle button for charging sounds, when
// plugging in a charger, the device won't play any charging sound.
TEST_F(PowerSoundsControllerTest, NoChargingSoundPlayedIfToggleButtonDisabled) {
  local_state()->SetBoolean(prefs::kChargingSoundsEnabled, false);
  ASSERT_FALSE(local_state()->GetBoolean(prefs::kChargingSoundsEnabled));

  // Charge the device after disabling the button, and no sounds will be played.
  SetPowerStatus(5, kAcPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());
}

// Tests if the warning sound can be played when the battery level drops below
// the threshold when connecting a low-power charger.
TEST_F(PowerSoundsControllerTest, PlayLowBatterySoundForPercentage) {
  // Don't play warning sound if the battery level is no less than the low power
  // threshold.
  SetPowerStatus(kLowPowerPercentage + 1, kUsbPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery drops below the low power threshold at the first time, the
  // device should play the sound for warning.
  SetPowerStatus(kLowPowerPercentage, kUsbPower);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  // When the battery level keeps dropping but no less than the critical power
  // threshold, the device shouldn't play sound for warning.
  SetPowerStatus(kLowPowerPercentage - 1, kUsbPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // The device will play the sound if its battery level keeps dropping to the
  // critical power threshold.
  SetPowerStatus(kCriticalPercentage, kUsbPower);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
}

// Tests if the warning sound can be played when the battery level drops below
// the threshold when disconnecting a line power.
TEST_F(PowerSoundsControllerTest, PlayLowBatterySoundForRemainingTime) {
  // Set the remaining minutes value higher than the low power threshold.
  SetPowerStatus(50, kDisconnectedPower, kLowPowerMinutes + 1);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // Set the remaining minutes value to the low power threshold.
  SetPowerStatus(50, kDisconnectedPower, kLowPowerMinutes);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  // When the rounded value keeps dropping but no less than the critical power
  // threshold, the device shouldn't play sound for warning.
  SetPowerStatus(50, kDisconnectedPower, kCriticalMinutes + 1);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // Set the rounded value lower than the critical power threshold.
  SetPowerStatus(50, kDisconnectedPower, kCriticalMinutes);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
}

// When the toggle button for the low battery sound is disabled, the sound won't
// be played if the battery drops below 15% when connecting with a low-power
// charger.
TEST_F(PowerSoundsControllerTest,
       NoLowBatterySoundPlayedIfToggleButtonDisabled) {
  local_state()->SetBoolean(prefs::kLowBatterySoundEnabled, false);
  ASSERT_FALSE(local_state()->GetBoolean(prefs::kLowBatterySoundEnabled));

  // Don't play warning sound if the battery level is no less than 15% when
  // connecting with a low-power charger.
  SetPowerStatus(16, kUsbPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery drops below 15% at the first time, e.g. 15%, the device
  // shouldn't play the sound for warning.
  SetPowerStatus(15, kUsbPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());
}

// Tests that the charging sound and the low battery sound will be played
// sequentially, because the charging sound is only played if connecting with an
// AC charger; however, the low battery sound won't be played if it's AC
// charger.
TEST_F(PowerSoundsControllerTest, PlaySoundsSequentially) {
  // 1. Tests that the low power minutes threshold come first, and then charging
  // the device.
  SetPowerStatus(10, kDisconnectedPower, kLowPowerMinutes + 1);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  SetPowerStatus(10, kDisconnectedPower, kLowPowerMinutes);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  SetPowerStatus(10, kAcPower, kLowPowerMinutes);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  // 2. Tests that charging the device first and then disconnecting the device
  // at the threshold.
  SetPowerStatus(10, kDisconnectedPower, kLowPowerMinutes + 1);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  SetPowerStatus(10, kAcPower, kLowPowerMinutes);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  SetPowerStatus(10, kDisconnectedPower, kLowPowerMinutes);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
}

// Tests that the recording when the device is plugged in or Unplugged are
// recorded correctly.
TEST_F(PowerSoundsControllerTest,
       RecordingBatteryLevelWhenPluggedInOrUnplugged) {
  SetPowerStatus(5, kAcPower);

  SetPowerStatus(50, kDisconnectedPower);
  SetPowerStatus(50, kAcPower);

  SetPowerStatus(100, kDisconnectedPower);
  SetPowerStatus(100, kAcPower);

  SetPowerStatus(100, kDisconnectedPower);
  SetPowerStatus(100, kAcPower);

  SetPowerStatus(100, kDisconnectedPower);

  // Compare the number of samples for battery level from 0% to 100%.
  for (int i = 0; i <= 100; i++) {
    histogram_tester_.ExpectBucketCount(
        PowerSoundsController::kPluggedInBatteryLevelHistogramName, i,
        plugged_in_levels_samples_[i]);

    histogram_tester_.ExpectBucketCount(
        PowerSoundsController::kUnpluggedBatteryLevelHistogramName, i,
        unplugged_levels_samples_[i]);
  }
}

// Tests that the sounds can only be played if the lid is opened; otherwise, we
// don't play any sounds.
TEST_F(PowerSoundsControllerTest, PlaySoundsOnlyIfLidIsOpened) {
  // When the lid is closed, plugging in a ac charger, the device don't play any
  // sound.
  EXPECT_TRUE(SetLidState(/*closed=*/true));
  SetPowerStatus(5, kAcPower);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When we open the lid, it doesn't play the delayed sound.
  EXPECT_FALSE(SetLidState(/*closed=*/false));
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // Under the condition of the lid opening, the device will play a sound when
  // charging it.
  SetPowerStatus(10, kDisconnectedPower);
  SetPowerStatus(5, kAcPower);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));
}

}  // namespace ash
