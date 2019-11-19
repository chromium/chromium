// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker.h"

#include <memory>

#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/biod/fake_biod_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace chromeos {
namespace {

constexpr char kFingerprint[] = "pinky";

class ScreenLockerTest : public InProcessBrowserTest {
 public:
  ScreenLockerTest() = default;
  ~ScreenLockerTest() override = default;

  FakeSessionManagerClient* session_manager_client() {
    return FakeSessionManagerClient::Get();
  }

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void TearDown() override { quick_unlock::EnabledForTesting(false); }

  void EnrollFingerprint() {
    quick_unlock::EnabledForTesting(true);

    FakeBiodClient::Get()->StartEnrollSession(
        "test-user", std::string(),
        base::BindRepeating(&ScreenLockerTest::OnStartSession,
                            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    FakeBiodClient::Get()->SendEnrollScanDone(
        kFingerprint, biod::SCAN_RESULT_SUCCESS, true /* is_complete */,
        -1 /* percent_complete */);
    base::RunLoop().RunUntilIdle();

    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetInteger(
        prefs::kQuickUnlockFingerprintRecord, 1);
  }

  void AuthenticateWithFingerprint() {
    FakeBiodClient::Get()->SendAuthScanDone(kFingerprint,
                                            biod::SCAN_RESULT_SUCCESS);
    base::RunLoop().RunUntilIdle();
  }

 private:
  void OnStartSession(const dbus::ObjectPath& path) {}

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestBadThenGoodPassword) {
  ScreenLockerTester tester;
  tester.Lock();

  tester.SetUnlockPassword(user_manager::StubAccountId(), "pass");

  // Submit a bad password.
  tester.UnlockWithPassword(user_manager::StubAccountId(), "fail");
  EXPECT_TRUE(tester.IsLocked());

  // Submit the correct password. Successful authentication clears the lock
  // screen and tells the SessionManager to announce this over DBus.
  tester.UnlockWithPassword(user_manager::StubAccountId(), "pass");
  EXPECT_FALSE(tester.IsLocked());
  EXPECT_EQ(1, session_manager_client()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(
      1, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

// Makes sure Chrome doesn't crash if we lock the screen during an add-user
// flow. Regression test for crbug.com/467111.
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, LockScreenWhileAddingUser) {
  UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();
  ScreenLocker::HandleShowLockScreenRequest();
}

// Test how locking the screen affects an active fullscreen window.
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestFullscreenExit) {
  // 1) If the active browser window is in fullscreen and the fullscreen window
  // does not have all the pixels (e.g. the shelf is auto hidden instead of
  // hidden), locking the screen should not exit fullscreen. The shelf is
  // auto hidden when in immersive fullscreen.
  ScreenLockerTester tester;
  BrowserWindow* browser_window = browser()->window();
  ash::WindowState* window_state =
      ash::WindowState::Get(browser_window->GetNativeWindow());
  {
    FullscreenNotificationObserver fullscreen_waiter(browser());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->ToggleBrowserFullscreenMode();
    fullscreen_waiter.Wait();
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_FALSE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_FALSE(tester.IsLocked());
  }
  {
    tester.Lock();
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_FALSE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_TRUE(tester.IsLocked());
  }
  tester.SetUnlockPassword(user_manager::StubAccountId(), "pass");
  tester.UnlockWithPassword(user_manager::StubAccountId(), "pass");
  EXPECT_FALSE(tester.IsLocked());
  {
    FullscreenNotificationObserver fullscreen_waiter(browser());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->ToggleBrowserFullscreenMode();
    fullscreen_waiter.Wait();
    EXPECT_FALSE(browser_window->IsFullscreen());
  }

  // Browser window should be activated after screen locker is gone. Otherwise,
  // the rest of the test would fail.
  ASSERT_EQ(window_state, ash::WindowState::ForActiveWindow());

  // 2) If the active browser window is in fullscreen and the fullscreen window
  // has all of the pixels, locking the screen should exit fullscreen. The
  // fullscreen window has all of the pixels when in tab fullscreen.
  {
    FullscreenNotificationObserver fullscreen_waiter(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    fullscreen_waiter.Wait();
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_TRUE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_FALSE(tester.IsLocked());
  }
  {
    tester.Lock();
    EXPECT_FALSE(browser_window->IsFullscreen());
    EXPECT_TRUE(tester.IsLocked());
  }

  tester.SetUnlockPassword(user_manager::StubAccountId(), "pass");
  tester.UnlockWithPassword(user_manager::StubAccountId(), "pass");
  EXPECT_FALSE(tester.IsLocked());

  EXPECT_EQ(2, session_manager_client()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(
      2, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestShowTwice) {
  ScreenLockerTester tester;
  tester.Lock();

  // Calling Show again simply send LockCompleted signal.
  ScreenLocker::Show();
  EXPECT_TRUE(tester.IsLocked());
  EXPECT_EQ(2, session_manager_client()->notify_lock_screen_shown_call_count());

  // Close the locker to match expectations.
  ScreenLocker::Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester.IsLocked());
  EXPECT_EQ(
      1, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, PasswordAuthWhenAuthDisabled) {
  // Show lock screen and wait until it is shown.
  ScreenLockerTester tester;
  tester.Lock();

  // Inject fake authentication credentials.
  const std::string kPassword = "pass";
  tester.SetUnlockPassword(user_manager::StubAccountId(), kPassword);
  EXPECT_TRUE(tester.IsLocked());

  // Disable authentication for user.
  ScreenLocker::default_screen_locker()->DisableAuthForUser(
      user_manager::StubAccountId(),
      ash::AuthDisabledData(ash::AuthDisabledReason::kTimeWindowLimit,
                            base::Time::Now() + base::TimeDelta::FromHours(1),
                            base::TimeDelta::FromHours(1),
                            true /*disable_lock_screen_media*/));

  // Try to authenticate with password.
  tester.UnlockWithPassword(user_manager::StubAccountId(), kPassword);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tester.IsLocked());

  // Re-enable authentication for user.
  ScreenLocker::default_screen_locker()->EnableAuthForUser(
      user_manager::StubAccountId());

  // Try to authenticate with password.
  tester.UnlockWithPassword(user_manager::StubAccountId(), kPassword);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester.IsLocked());
  EXPECT_EQ(1, session_manager_client()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(
      1, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, FingerprintAuthWhenAuthDisabled) {
  EnrollFingerprint();

  // Show lock screen and wait until it is shown.
  ScreenLockerTester tester;
  tester.Lock();

  const std::string kPassword = "pass";
  tester.SetUnlockPassword(user_manager::StubAccountId(), kPassword);
  EXPECT_TRUE(tester.IsLocked());

  // Disable authentication for user.
  ScreenLocker::default_screen_locker()->DisableAuthForUser(
      user_manager::StubAccountId(),
      ash::AuthDisabledData(ash::AuthDisabledReason::kTimeUsageLimit,
                            base::Time::Now() + base::TimeDelta::FromHours(1),
                            base::TimeDelta::FromHours(3),
                            true /*disable_lock_screen_media*/));

  // Try to authenticate with fingerprint.
  AuthenticateWithFingerprint();
  EXPECT_TRUE(tester.IsLocked());

  // Re-enable authentication for user.
  ScreenLocker::default_screen_locker()->EnableAuthForUser(
      user_manager::StubAccountId());

  // Try to authenticate with fingerprint.
  AuthenticateWithFingerprint();
  EXPECT_FALSE(tester.IsLocked());
  EXPECT_EQ(1, session_manager_client()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(
      1, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

}  // namespace chromeos
