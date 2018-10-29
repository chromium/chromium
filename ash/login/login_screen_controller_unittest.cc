// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"

using ::testing::_;
using namespace session_manager;

namespace ash {

namespace {
using LoginScreenControllerTest = AshTestBase;
using LoginScreenControllerNoSessionTest = NoSessionAshTestBase;

// Enum instead of enum class, because it is used for indexing.
enum WindowType { kPrimary = 0, kSecondary = 1 };

bool IsSystemTrayForWindowVisible(WindowType index) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  RootWindowController* controller =
      RootWindowController::ForWindow(root_windows[index]);
  return controller->GetStatusAreaWidget()->unified_system_tray()->visible();
}

TEST_F(LoginScreenControllerTest, RequestAuthentication) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  std::string password = "password";
  // Verify AuthenticateUser mojo call is run with the same account id, a
  // (hashed) password, and the correct PIN state.
  EXPECT_CALL(*client,
              AuthenticateUserWithPasswordOrPin_(id, password, false, _));
  base::Optional<bool> callback_result;
  base::RunLoop run_loop1;
  controller->AuthenticateUserWithPasswordOrPin(
      id, password, false,
      base::BindLambdaForTesting([&](base::Optional<bool> did_auth) {
        callback_result = did_auth;
        run_loop1.Quit();
      }));
  run_loop1.Run();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);

  // Verify that pin is hashed correctly.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kQuickUnlockPinSalt));

  std::string pin = "123456";
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(id, pin, true, _));
  base::RunLoop run_loop2;
  controller->AuthenticateUserWithPasswordOrPin(
      id, pin, true,
      base::BindLambdaForTesting([&](base::Optional<bool> did_auth) {
        callback_result = did_auth;
        run_loop2.Quit();
      }));
  run_loop2.Run();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);
}

TEST_F(LoginScreenControllerTest, RequestEasyUnlock) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // Verify AttemptUnlock mojo call is run with the same account id.
  EXPECT_CALL(*client, AuthenticateUserWithEasyUnlock(id));
  controller->AuthenticateUserWithEasyUnlock(id);
  base::RunLoop().RunUntilIdle();

  // Verify HardlockPod mojo call is run with the same account id.
  EXPECT_CALL(*client, HardlockPod(id));
  controller->HardlockPod(id);
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginScreenControllerTest, RequestUserPodFocus) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // Verify FocusPod mojo call is run with the same account id.
  EXPECT_CALL(*client, OnFocusPod(id));
  controller->OnFocusPod(id);
  base::RunLoop().RunUntilIdle();

  // Verify NoPodFocused mojo call is run.
  EXPECT_CALL(*client, OnNoPodFocused());
  controller->OnNoPodFocused();
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginScreenControllerTest,
       ShowLoginScreenRequiresLoginPrimarySessionState) {
  auto show_login = [&](session_manager::SessionState state) {
    EXPECT_FALSE(ash::LockScreen::HasInstance());

    LoginScreenController* controller = Shell::Get()->login_screen_controller();

    GetSessionControllerClient()->SetSessionState(state);
    base::Optional<bool> result;
    base::RunLoop run_loop;
    controller->ShowLoginScreen(base::BindOnce(
        [](base::Optional<bool>* result, base::RunLoop* run_loop,
           bool did_show) {
          *result = did_show;
          run_loop->Quit();
        },
        &result, &run_loop));
    run_loop.Run();

    EXPECT_TRUE(result.has_value());

    // Verify result matches actual ash::LockScreen state.
    EXPECT_EQ(*result, ash::LockScreen::HasInstance());

    // Destroy login if we created it.
    if (*result)
      ash::LockScreen::Get()->Destroy();

    return *result;
  };

  EXPECT_FALSE(show_login(session_manager::SessionState::UNKNOWN));
  EXPECT_FALSE(show_login(session_manager::SessionState::OOBE));
  EXPECT_TRUE(show_login(session_manager::SessionState::LOGIN_PRIMARY));
  EXPECT_FALSE(show_login(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE));
  EXPECT_FALSE(show_login(session_manager::SessionState::ACTIVE));
  EXPECT_FALSE(show_login(session_manager::SessionState::LOCKED));
  EXPECT_FALSE(show_login(session_manager::SessionState::LOGIN_SECONDARY));
}

TEST_F(LoginScreenControllerNoSessionTest, ShowSystemTrayOnPrimaryLoginScreen) {
  // Create setup with 2 displays primary and secondary.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  EXPECT_FALSE(ash::LockScreen::HasInstance());
  EXPECT_FALSE(IsSystemTrayForWindowVisible(WindowType::kPrimary));
  EXPECT_FALSE(IsSystemTrayForWindowVisible(WindowType::kSecondary));

  // Show login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  base::Optional<bool> result;
  base::RunLoop run_loop;
  Shell::Get()->login_screen_controller()->ShowLoginScreen(base::BindOnce(
      [](base::Optional<bool>* result, base::RunLoop* run_loop, bool did_show) {
        *result = did_show;
        run_loop->Quit();
      },
      &result, &run_loop));
  run_loop.Run();
  EXPECT_TRUE(result.has_value());

  EXPECT_TRUE(ash::LockScreen::HasInstance());
  EXPECT_TRUE(IsSystemTrayForWindowVisible(WindowType::kPrimary));
  EXPECT_FALSE(IsSystemTrayForWindowVisible(WindowType::kSecondary));

  if (*result)
    ash::LockScreen::Get()->Destroy();
}

TEST_F(LoginScreenControllerTest, ShowSystemTrayOnPrimaryLockScreen) {
  // Create setup with 2 displays primary and secondary.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(ash::LockScreen::HasInstance());
  EXPECT_TRUE(IsSystemTrayForWindowVisible(WindowType::kPrimary));
  EXPECT_TRUE(IsSystemTrayForWindowVisible(WindowType::kSecondary));

  // Show lock screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);
  base::Optional<bool> result;
  base::RunLoop run_loop;
  Shell::Get()->login_screen_controller()->ShowLockScreen(base::BindOnce(
      [](base::Optional<bool>* result, base::RunLoop* run_loop, bool did_show) {
        *result = did_show;
        run_loop->Quit();
      },
      &result, &run_loop));
  run_loop.Run();
  EXPECT_TRUE(result.has_value());

  EXPECT_TRUE(ash::LockScreen::HasInstance());
  EXPECT_TRUE(IsSystemTrayForWindowVisible(WindowType::kPrimary));
  EXPECT_FALSE(IsSystemTrayForWindowVisible(WindowType::kSecondary));

  if (*result)
    ash::LockScreen::Get()->Destroy();
}

TEST_F(LoginScreenControllerTest, ShowLoginScreenRequiresWallpaper) {
  // Show login screen.
  EXPECT_FALSE(ash::LockScreen::HasInstance());
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  base::RunLoop run_loop;
  Shell::Get()->login_screen_controller()->ShowLoginScreen(base::BindOnce(
      [](base::RunLoop* run_loop, bool did_show) { run_loop->Quit(); },
      &run_loop));
  run_loop.Run();

  // Verify the instance has been created, but the login screen is not actually
  // shown yet because there's no wallpaper.
  EXPECT_TRUE(ash::LockScreen::HasInstance());
  EXPECT_FALSE(ash::LockScreen::Get()->is_shown());

  // Set the wallpaper. Verify the login screen is shown.
  Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ash::LockScreen::Get()->is_shown());

  ash::LockScreen::Get()->Destroy();
}

}  // namespace
}  // namespace ash
