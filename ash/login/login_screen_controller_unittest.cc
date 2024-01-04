// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "testing/gmock/include/gmock/gmock.h"

using session_manager::SessionState;
using ::testing::_;

namespace ash {

class LoginScreenControllerTest : public AshTestBase {
 public:
  LoginScreenControllerTest() {
    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
  }

  ToastOverlay* GetCurrentToast() {
    aura::Window* root_window = Shell::GetRootWindowForNewWindows();
    return Shell::Get()->toast_manager()->GetCurrentOverlayForTesting(
        root_window);
  }

 private:
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
};

class LoginScreenControllerNoSessionTest : public NoSessionAshTestBase {
 public:
  LoginScreenControllerNoSessionTest() {
    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
  }

 private:
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
};

// Enum instead of enum class, because it is used for indexing.
enum WindowType { kPrimary = 0, kSecondary = 1 };

TEST_F(LoginScreenControllerTest, RequestAuthentication) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  auto client = std::make_unique<MockLoginScreenClient>();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  std::string password = "password";
  // Verify AuthenticateUser mojo call is run with the same account id, a
  // (hashed) password, and the correct PIN state.
  EXPECT_CALL(*client,
              AuthenticateUserWithPasswordOrPin_(id, password, false, _));
  std::optional<bool> callback_result;
  base::RunLoop run_loop1;
  controller->AuthenticateUserWithPasswordOrPin(
      id, password, false,
      base::BindLambdaForTesting([&](std::optional<bool> did_auth) {
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

  // Use a long PIN (N > 2^64) for the test to ensure that there is no overflow.
  std::string pin = "12345678901234567890";
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(id, pin, true, _));
  base::RunLoop run_loop2;
  controller->AuthenticateUserWithPasswordOrPin(
      id, pin, true,
      base::BindLambdaForTesting([&](std::optional<bool> did_auth) {
        callback_result = did_auth;
        run_loop2.Quit();
      }));
  run_loop2.Run();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);
}

TEST_F(LoginScreenControllerTest, RequestEasyUnlock) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  auto client = std::make_unique<MockLoginScreenClient>();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // Verify AttemptUnlock mojo call is run with the same account id.
  EXPECT_CALL(*client, AuthenticateUserWithEasyUnlock(id));
  controller->AuthenticateUserWithEasyUnlock(id);
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginScreenControllerTest, RequestUserPodFocus) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  auto client = std::make_unique<MockLoginScreenClient>();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // Verify FocusPod mojo call is run with the same account id.
  EXPECT_CALL(*client, OnFocusPod(id));
  controller->OnFocusPod(id);
  base::RunLoop().RunUntilIdle();
}

// b/308840749 test for clicking on second user pod while first is logging in.
TEST_F(LoginScreenControllerNoSessionTest, DoesNotCallOnFocusPodDuringLogin) {
  ASSERT_EQ(SessionState::LOGIN_PRIMARY,
            Shell::Get()->session_controller()->GetSessionState());

  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  auto client = std::make_unique<testing::StrictMock<MockLoginScreenClient>>();
  AccountId id = AccountId::FromUserEmail("user1@test.com");
  EXPECT_CALL(*client, OnFocusPod(id)).Times(1);
  controller->OnFocusPod(id);

  // Simulate starting log in as user1.
  GetSessionControllerClient()->SetSessionState(
      SessionState::LOGGED_IN_NOT_ACTIVE);

  // Click on user2 pod while logging in as user1. No `OnFocusPod` calls sent.
  controller->OnFocusPod(AccountId::FromUserEmail("user2@test.com"));
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginScreenControllerNoSessionTest, ShowSystemTrayOnPrimaryLoginScreen) {
  // Create setup with 2 displays primary and secondary.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  EXPECT_FALSE(ash::LockScreen::HasInstance());
  EXPECT_FALSE(IsSystemTrayForRootWindowVisible(WindowType::kPrimary));
  EXPECT_FALSE(IsSystemTrayForRootWindowVisible(WindowType::kSecondary));

  // Show login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  Shell::Get()->login_screen_controller()->ShowLoginScreen();

  EXPECT_TRUE(ash::LockScreen::HasInstance());
  EXPECT_TRUE(IsSystemTrayForRootWindowVisible(WindowType::kPrimary));
  EXPECT_FALSE(IsSystemTrayForRootWindowVisible(WindowType::kSecondary));

  ash::LockScreen::Get()->Destroy();
}

TEST_F(LoginScreenControllerNoSessionTest,
       SystemTrayVisibilityOnSecondaryScreenRestored) {
  // Create setup with 2 displays primary and secondary.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Show login screen, then hide it.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  Shell::Get()->login_screen_controller()->ShowLoginScreen();
  ash::LockScreen::Get()->Destroy();

  // The system tray should be visible on the secondary screen.
  EXPECT_TRUE(IsSystemTrayForRootWindowVisible(WindowType::kSecondary));
}

TEST_F(LoginScreenControllerTest, ShowSystemTrayOnPrimaryLockScreen) {
  // Create setup with 2 displays primary and secondary.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(ash::LockScreen::HasInstance());
  EXPECT_TRUE(IsSystemTrayForRootWindowVisible(WindowType::kPrimary));
  EXPECT_TRUE(IsSystemTrayForRootWindowVisible(WindowType::kSecondary));

  // Show lock screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);
  Shell::Get()->login_screen_controller()->ShowLockScreen();

  EXPECT_TRUE(ash::LockScreen::HasInstance());
  EXPECT_TRUE(IsSystemTrayForRootWindowVisible(WindowType::kPrimary));
  EXPECT_FALSE(IsSystemTrayForRootWindowVisible(WindowType::kSecondary));

  ash::LockScreen::Get()->Destroy();
}

TEST_F(LoginScreenControllerTest, ShowLoginScreenRequiresWallpaper) {
  // Show login screen.
  EXPECT_FALSE(ash::LockScreen::HasInstance());
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  Shell::Get()->login_screen_controller()->ShowLoginScreen();

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

TEST_F(LoginScreenControllerTest, SystemTrayFocus) {
  auto client = std::make_unique<MockLoginScreenClient>();

  EXPECT_CALL(*client, OnFocusLeavingSystemTray(true)).Times(1);
  Shell::Get()->system_tray_notifier()->NotifyFocusOut(true);

  EXPECT_CALL(*client, OnFocusLeavingSystemTray(false)).Times(1);
  Shell::Get()->system_tray_notifier()->NotifyFocusOut(false);
}

TEST_F(LoginScreenControllerTest, KioskAppErrorToastIsDismissedOnDestroy) {
  EXPECT_EQ(GetCurrentToast(), nullptr);
  Shell::Get()->login_screen_controller()->ShowKioskAppError("Some error");

  auto* toast = GetCurrentToast();
  ASSERT_NE(toast, nullptr);
  ASSERT_EQ(toast->GetText(), u"Some error");

  Shell::Get()->login_screen_controller()->OnLockScreenDestroyed();
  EXPECT_EQ(GetCurrentToast(), nullptr);
}

}  // namespace ash
