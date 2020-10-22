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
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"

namespace ash {

constexpr char kUser1[] = "user1@gmail.com";
constexpr char kUser2[] = "user2@gmail.com";

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
  FastForwardTiny();

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
  FastForwardTiny();

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
  FastForwardTiny();

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
  FastForwardTiny();

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

TEST_F(AmbientControllerTest, CloseAmbientScreenUponUnlockSecondaryUser) {
  // Simulate the login screen.
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  SimulateUserLogin(kUser2);
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  EXPECT_FALSE(container_view());

  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  EXPECT_FALSE(container_view());
}

TEST_F(AmbientControllerTest, NotShowAmbientWhenLockSecondaryUser) {
  // Simulate the login screen.
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  SimulateUserLogin(kUser2);
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  EXPECT_FALSE(container_view());

  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();

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

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenAfterFailure) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Token request automatically retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenWithBackoffPolicy) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  base::TimeDelta delay1 = GetRefreshTokenDelay();
  task_environment()->FastForwardBy(delay1 * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  base::TimeDelta delay2 = GetRefreshTokenDelay();
  EXPECT_GT(delay2, delay1);

  task_environment()->FastForwardBy(delay2 * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenOnlyThreeTimes) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 1st retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 2nd retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 3rd retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Will not retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryIsCharging) {
  // Simulate a device being connected to a charger initially.
  SetPowerStateCharging();

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();

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
  SetPowerStateDischarging();
  // Lock screen to start ambient mode.
  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();

  EXPECT_TRUE(ambient_controller()->IsShown());
  // Should not acquire wake lock when device is not charging.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Connect the device with a charger.
  SetPowerStateCharging();
  base::RunLoop().RunUntilIdle();

  // Should acquire the wake lock when battery is charging.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates a full battery.
  SetPowerStateFull();

  // Should keep the wake lock as the charger is still connected.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Disconnects the charger again.
  SetPowerStateDischarging();
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when battery is not charging.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // An unbalanced release should do nothing.
  UnlockScreen();
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

// TODO(cowmoo): find a way to simulate events to trigger |UserActivityDetector|
TEST_F(AmbientControllerTest, ShouldDismissContainerViewOnEvents) {
  std::vector<std::unique_ptr<ui::Event>> events;

  for (auto mouse_event_type : {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_MOVED}) {
    events.emplace_back(std::make_unique<ui::MouseEvent>(
        mouse_event_type, gfx::Point(), gfx::Point(), base::TimeTicks(),
        ui::EF_NONE, ui::EF_NONE));
  }

  events.emplace_back(std::make_unique<ui::MouseWheelEvent>(
      gfx::Vector2d(), gfx::PointF(), gfx::PointF(), base::TimeTicks(),
      ui::EF_NONE, ui::EF_NONE));

  events.emplace_back(std::make_unique<ui::KeyEvent>(
      ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));

  events.emplace_back(std::make_unique<ui::ScrollEvent>(
      ui::ET_SCROLL, gfx::PointF(), gfx::PointF(), base::TimeTicks(),
      ui::EF_NONE, /*x_offset=*/0.0f,
      /*y_offset=*/0.0f,
      /*x_offset_ordinal=*/0.0f,
      /*x_offset_ordinal=*/0.0f, /*finger_count=*/2));

  events.emplace_back(std::make_unique<ui::TouchEvent>(
      ui::ET_TOUCH_PRESSED, gfx::PointF(), gfx::PointF(), base::TimeTicks(),
      ui::PointerDetails()));

  for (const auto& event : events) {
    ShowAmbientScreen();
    FastForwardTiny();
    EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

    ambient_controller()->OnUserActivity(event.get());

    EXPECT_FALSE(container_view());

    // Clean up.
    CloseAmbientScreen();
  }
}

TEST_F(AmbientControllerTest, ShouldDismissAndThenComesBack) {
  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::KeyboardCode::VKEY_1,
                         ui::EF_NONE);
  ambient_controller()->OnUserActivity(&key_event);
  EXPECT_FALSE(container_view());

  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToLockScreen();
  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWithNoisyPowerEvents) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardHalfLockScreenDelay();
  SetPowerStateCharging();

  FastForwardHalfLockScreenDelay();
  SetPowerStateCharging();

  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithoutLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  // When power is discharging, we do not lock the screen with ambient
  // mode since we do not prevent the device go to sleep which will natually
  // lock the device.
  SetPowerStateDischarging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToLockScreen();
  EXPECT_FALSE(IsLocked());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldShowAmbientScreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device but enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToLockScreen();
  EXPECT_FALSE(IsLocked());

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

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Should dismiss ambient mode screen.
  SetScreenBrightnessAndWait(/*percent=*/0);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldHideAmbientScreenWhenDisplayIsOffThenComesBackWithLockScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToLockScreen();
  EXPECT_TRUE(IsLocked());

  // Should dismiss ambient mode screen.
  SetScreenBrightnessAndWait(/*percent=*/0);
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen, but still has lock
  // screen.
  SetScreenBrightnessAndWait(/*percent=*/50);
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToInactivity();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, HideCursor) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  LockScreen();

  cursor_manager->ShowCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  FastForwardToInactivity();
  FastForwardTiny();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

}  // namespace ash
