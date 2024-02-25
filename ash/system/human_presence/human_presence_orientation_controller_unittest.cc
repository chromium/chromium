// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/human_presence_orientation_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {
namespace {

// A simple observer that lists its last observation.
class TestObserver : public HumanPresenceOrientationController::Observer {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // HumanPresenceOrientationController::Observer::
  void OnOrientationChanged(bool suitable_for_hps) override {
    ++observation_count_;
    last_observation_ = suitable_for_hps;
  }

  int observation_count() { return observation_count_; }

  int last_observation() { return last_observation_; }

 private:
  int observation_count_ = 0;
  bool last_observation_ = false;
};

class HumanPresenceOrientationControllerTest : public AshTestBase {
 public:
  HumanPresenceOrientationControllerTest() = default;
  HumanPresenceOrientationControllerTest(
      const HumanPresenceOrientationControllerTest&) = delete;
  HumanPresenceOrientationControllerTest& operator=(
      const HumanPresenceOrientationControllerTest&) = delete;
  ~HumanPresenceOrientationControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({ash::features::kSnoopingProtection},
                                          {ash::features::kQuickDim});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kHasHps);

    chromeos::PowerManagerClient::InitializeFake();
    AshTestBase::SetUp();

    orientation_controller_ =
        Shell::Get()->human_presence_orientation_controller();
    tablet_mode_controller_ = Shell::Get()->tablet_mode_controller();
    display_manager_ = Shell::Get()->display_manager();
    power_manager_client_ = AshTestBase::power_manager_client();

    // Two displays: the first internal and the second external.
    UpdateDisplay("800x600,1024x768");
    display::test::DisplayManagerTestApi(display_manager_)
        .SetFirstDisplayAsInternalDisplay();
  }

 protected:
  void RotateDisplay(int display_index, int degrees) {
    const int64_t id = display_manager_->GetDisplayAt(display_index).id();
    display_manager_->SetDisplayRotation(
        id, display::Display::DegreesToRotation(degrees),
        display::Display::RotationSource::ACTIVE);
  }

  raw_ptr<HumanPresenceOrientationController, DanglingUntriaged>
      orientation_controller_ = nullptr;
  raw_ptr<TabletModeController, DanglingUntriaged> tablet_mode_controller_ =
      nullptr;
  raw_ptr<display::DisplayManager, DanglingUntriaged> display_manager_ =
      nullptr;
  raw_ptr<chromeos::FakePowerManagerClient, DanglingUntriaged>
      power_manager_client_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(HumanPresenceOrientationControllerTest, TabletMode) {
  ASSERT_TRUE(orientation_controller_->IsOrientationSuitable());

  tablet_mode_controller_->SetEnabledForTest(true);
  EXPECT_FALSE(orientation_controller_->IsOrientationSuitable());
  tablet_mode_controller_->SetEnabledForTest(false);
  EXPECT_TRUE(orientation_controller_->IsOrientationSuitable());
}

TEST_F(HumanPresenceOrientationControllerTest, DisplayOrientation) {
  ASSERT_TRUE(orientation_controller_->IsOrientationSuitable());

  // Rotating the external display has no effect on our sensor.
  RotateDisplay(/*display_index=*/1, /*degrees=*/90);
  EXPECT_TRUE(orientation_controller_->IsOrientationSuitable());

  // Rotating the internal display _does_ have an effect on our sensor.
  RotateDisplay(/*display_index=*/0, /*degrees=*/270);
  EXPECT_FALSE(orientation_controller_->IsOrientationSuitable());

  RotateDisplay(/*display_index=*/1, /*degrees=*/0);
  EXPECT_FALSE(orientation_controller_->IsOrientationSuitable());

  RotateDisplay(/*display_index=*/0, /*degrees=*/0);
  EXPECT_TRUE(orientation_controller_->IsOrientationSuitable());
}

TEST_F(HumanPresenceOrientationControllerTest, LidState) {
  ASSERT_TRUE(orientation_controller_->IsOrientationSuitable());
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, base::TimeTicks());
  ASSERT_FALSE(orientation_controller_->IsOrientationSuitable());
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, base::TimeTicks());
  ASSERT_TRUE(orientation_controller_->IsOrientationSuitable());
}

TEST_F(HumanPresenceOrientationControllerTest, Observer) {
  TestObserver observer;
  orientation_controller_->AddObserver(&observer);

  ASSERT_TRUE(orientation_controller_->IsOrientationSuitable());
  ASSERT_EQ(observer.observation_count(), 0);

  // Rotating the external display has no effect on our sensor, so no event
  // should fire.
  RotateDisplay(/*display_index=*/1, /*degrees=*/90);
  EXPECT_EQ(observer.observation_count(), 0);

  // Entering tablet mode should explicitly notify observers.
  tablet_mode_controller_->SetEnabledForTest(true);
  EXPECT_EQ(observer.observation_count(), 1);
  EXPECT_EQ(observer.last_observation(), /*suitable_for_hps=*/false);

  // While rotating the internal display makes the device unsuitable, no event
  // should fire because the suitability hasn't _changed_.
  RotateDisplay(/*display_index=*/0, /*degrees=*/90);
  EXPECT_EQ(observer.observation_count(), 1);
  EXPECT_EQ(observer.last_observation(), /*suitable_for_hps=*/false);

  tablet_mode_controller_->SetEnabledForTest(false);
  RotateDisplay(/*display_index=*/0, /*degrees=*/0);
  EXPECT_EQ(observer.observation_count(), 2);
  EXPECT_EQ(observer.last_observation(), /*suitable_for_hps=*/true);

  // Closing the device lid makes the device unsuitable.
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, base::TimeTicks());
  EXPECT_EQ(observer.observation_count(), 3);
  ASSERT_FALSE(observer.last_observation());
  // Opening the device lid makes the device suitable.
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, base::TimeTicks());
  EXPECT_EQ(observer.observation_count(), 4);
  EXPECT_TRUE(observer.last_observation());

  orientation_controller_->RemoveObserver(&observer);
}

}  // namespace
}  // namespace ash
