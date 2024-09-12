// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {
namespace {

constexpr char kFingerprint[] = "pinky";

class ScreenLockerTest : public InProcessBrowserTest {
 public:
  ScreenLockerTest() = default;

  ScreenLockerTest(const ScreenLockerTest&) = delete;
  ScreenLockerTest& operator=(const ScreenLockerTest&) = delete;

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

  void EnrollFingerprint() {
    test_api_ = std::make_unique<quick_unlock::TestApi>(
        /*override_quick_unlock=*/true);
    test_api_->EnableFingerprintByPolicy(quick_unlock::Purpose::kUnlock);

    FakeBiodClient::Get()->StartEnrollSession(
        "test-user", std::string(),
        base::BindOnce(&ScreenLockerTest::OnStartSession,
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
    biod::FingerprintMessage msg;
    msg.set_scan_result(biod::SCAN_RESULT_SUCCESS);
    FakeBiodClient::Get()->SendAuthScanDone(kFingerprint, msg);
    base::RunLoop().RunUntilIdle();
  }

 private:
  void OnStartSession(const dbus::ObjectPath& path) {}

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  std::unique_ptr<quick_unlock::TestApi> test_api_;
};

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

// Test how locking the screen affects an active fullscreen window.
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestFullscreenExit) {
  // 1) If the active browser window is in fullscreen and the fullscreen window
  // does not have all the pixels (e.g. the shelf is auto hidden instead of
  // hidden), locking the screen should exit fullscreen. The shelf is
  // auto hidden when in immersive fullscreen.
  ScreenLockerTester tester;
  BrowserWindow* browser_window = browser()->window();
  auto* window_state = WindowState::Get(browser_window->GetNativeWindow());
  {
    ui_test_utils::ToggleFullscreenModeAndWait(browser());
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_FALSE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_FALSE(tester.IsLocked());
  }
  {
    tester.Lock();
    EXPECT_FALSE(browser_window->IsFullscreen());
    EXPECT_TRUE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_TRUE(tester.IsLocked());
  }
  tester.SetUnlockPassword(user_manager::StubAccountId(), "pass");
  tester.UnlockWithPassword(user_manager::StubAccountId(), "pass");
  EXPECT_FALSE(tester.IsLocked());
  EXPECT_FALSE(browser_window->IsFullscreen());

  // Browser window should be activated after screen locker is gone. Otherwise,
  // the rest of the test would fail.
  ASSERT_EQ(window_state, WindowState::ForActiveWindow());

  // 2) Similar to 1) if the active browser window is in fullscreen and the
  // fullscreen window has all of the pixels, locking the screen should exit
  // fullscreen. The fullscreen window has all of the pixels when in tab
  // fullscreen.
  {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents->GetPrimaryMainFrame());
    waiter.Wait();
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
  ScreenLocker::default_screen_locker()->TemporarilyDisableAuthForUser(
      user_manager::StubAccountId(),
      AuthDisabledData(AuthDisabledReason::kTimeWindowLimit,
                       base::Time::Now() + base::Hours(1), base::Hours(1),
                       true /*disable_lock_screen_media*/));

  // Try to authenticate with password.
  tester.ForceSubmitPassword(user_manager::StubAccountId(), kPassword);
  EXPECT_TRUE(tester.IsLocked());

  // Re-enable authentication for user.
  ScreenLocker::default_screen_locker()->ReenableAuthForUser(
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
  ScreenLocker::default_screen_locker()->TemporarilyDisableAuthForUser(
      user_manager::StubAccountId(),
      AuthDisabledData(AuthDisabledReason::kTimeUsageLimit,
                       base::Time::Now() + base::Hours(1), base::Hours(3),
                       true /*disable_lock_screen_media*/));

  // Try to authenticate with fingerprint.
  AuthenticateWithFingerprint();
  EXPECT_TRUE(tester.IsLocked());

  // Re-enable authentication for user.
  ScreenLocker::default_screen_locker()->ReenableAuthForUser(
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

}  // namespace
}  // namespace ash
