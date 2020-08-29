// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/system/power/power_status.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class AmbientControllerTest : public AmbientAshTestBase {
 public:
  AmbientControllerTest() : AmbientAshTestBase() {}
  ~AmbientControllerTest() override = default;

  // AmbientAshTestBase:
  void SetUp() override {
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
  }
};

TEST_F(AmbientControllerTest, ShowAmbientScreenUponLock) {
  LockScreen();
  // Lockscreen will not immediately show Ambient mode.
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Ambient mode will show after inacivity and successfully loading first
  // image.
  FastForwardToInactivity();
  FastForwardToNextImage();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, NotShowAmbientWhenPrefNotEnabled) {
  SetAmbientModeEnabled(false);

  LockScreen();
  // Lockscreen will not immediately show Ambient mode.
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Ambient mode will not show after inacivity and successfully loading first
  // image.
  FastForwardToInactivity();
  FastForwardToNextImage();

  EXPECT_FALSE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, HideAmbientScreen) {
  LockScreen();
  FastForwardToInactivity();
  FastForwardToNextImage();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  HideAmbientScreen();

  EXPECT_FALSE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kHidden);

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, CloseAmbientScreenUponUnlock) {
  LockScreen();
  FastForwardToInactivity();
  FastForwardToNextImage();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  UnlockScreen();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  EXPECT_FALSE(container_view());
}

TEST_F(AmbientControllerTest, ShouldRequestAccessTokenWhenLockingScreen) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Should close ambient widget already when unlocking screen.
  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldNotRequestAccessTokenWhenPrefNotEnabled) {
  SetAmbientModeEnabled(false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will not request a token.
  LockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());

  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldReturnCachedAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, access_token);

        std::move(closure).Run();
        run_loop.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop.Run();

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldReturnEmptyAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop_1;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, access_token);

        std::move(closure).Run();
        run_loop_1.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  // When token expired, another token request will get empty token.
  constexpr base::TimeDelta kTokenRefreshDelay =
      base::TimeDelta::FromSeconds(60);
  task_environment()->FastForwardBy(kTokenRefreshDelay);

  closure = base::MakeExpectedRunClosure(FROM_HERE);
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_TRUE(access_token_fetched.empty());

        std::move(closure).Run();
        run_loop_2.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop_2.Run();

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRefreshAccessTokenAfterFailure) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Token request automatically retry.
  // The failure delay has jitter so fast forward a bit more.
  constexpr base::TimeDelta kMaxTokenRefreshDelay =
      base::TimeDelta::FromSeconds(60);
  task_environment()->FastForwardBy(kMaxTokenRefreshDelay * 2);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryIsCharging) {
  // Simulate a device being connected to a charger initially.
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  PowerStatus::Get()->SetProtoForTesting(proto);

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardToInactivity();
  FastForwardToNextImage();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardToInactivity();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryStateChanged) {
  // Simulate a device being disconnected with a charger initially.
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(proto);
  // Lock screen to start ambient mode.
  LockScreen();
  FastForwardToInactivity();
  FastForwardToNextImage();

  EXPECT_TRUE(ambient_controller()->IsShown());
  // Should not acquire wake lock when device is not charging.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Connect the device with a charger.
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto);
  // Notify the controller about the power status change, and flush the loop to
  // ensure the wake lock request has reached the wake lock provider.
  ambient_controller()->OnPowerStatusChanged();
  base::RunLoop().RunUntilIdle();

  // Should acquire the wake lock when battery is charging.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates a full battery.
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();

  // Should keep the wake lock as the charger is still connected.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Disconnects the charger again.
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when battery is not charging.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // An unbalanced release should do nothing.
  UnlockScreen();
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest, ShouldDismissContainerViewWhenKeyPressed) {
  ShowAmbientScreen();
  FastForwardToNextImage();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

  // Simulates a random keyboard press event.
  GetEventGenerator()->PressKey(ui::VKEY_SPACE, /*flags=*/0);

  EXPECT_FALSE(container_view());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldDismissContainerViewOnRealMouseMove) {
  ShowAmbientScreen();
  FastForwardToNextImage();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

  // Simulates a tiny mouse move within the threshold, which should be ignored.
  GetEventGenerator()->MoveMouseBy(/*x=*/1, /*y=*/1);
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

  // Simulates a big mouse move beyond the threshold, which should take effect
  // and dismiss the ambient.
  GetEventGenerator()->MoveMouseBy(/*x=*/15, /*y=*/15);
  EXPECT_FALSE(container_view());
}

TEST_F(AmbientControllerTest, ShouldDismissAndThenComesBack) {
  LockScreen();
  FastForwardToInactivity();
  FastForwardToNextImage();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_1, ui::EF_NONE);
  EXPECT_FALSE(container_view());

  FastForwardToInactivity();
  FastForwardToNextImage();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should lock the device and enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToInactivity();
  FastForwardToNextImage();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, ShouldShowAmbientScreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device but enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardToNextImage();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldHideAmbientScreenWhenDisplayIsOff) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardToNextImage();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Should dismiss ambient mode screen.
  SetScreenBrightnessAndWait(/*percent=*/0);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardToNextImage();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  FastForwardToNextImage();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldHideAmbientScreenWhenDisplayIsOffThenComesBackWithLockScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_TRUE(IsLocked());

  FastForwardToInactivity();
  FastForwardToNextImage();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Should dismiss ambient mode screen.
  SetScreenBrightnessAndWait(/*percent=*/0);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardToInactivity();
  FastForwardToNextImage();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen, but still has lock
  // screen.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToInactivity();
  FastForwardToNextImage();
  EXPECT_TRUE(ambient_controller()->IsShown());
}

}  // namespace ash
