// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker.h"

#include "base/test/simple_test_clock.cc"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/biod/fake_biod_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace chromeos {
namespace {

using QuickUnlockStorage = quick_unlock::QuickUnlockStorage;

constexpr char kFingerprint[] = "pinky";

class FingerprintUnlockTest : public InProcessBrowserTest {
 public:
  FingerprintUnlockTest() = default;
  ~FingerprintUnlockTest() override = default;

  void SetUp() override {
    quick_unlock::EnabledForTesting(true);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    quick_unlock::EnabledForTesting(false);
    InProcessBrowserTest::TearDown();
  }

  void SetUpInProcessBrowserTestFixture() override {
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetupTestClocks();
    SetUpQuickUnlock();
    EnrollFingerprint();
  }

  void EnrollFingerprint() {
    FakeBiodClient::Get()->StartEnrollSession(
        "test-user", std::string(),
        base::BindRepeating(&FingerprintUnlockTest::OnStartSession,
                            base::Unretained(this)));
    if (!fingerprint_session_started_) {
      base::RunLoop run_loop;
      fingerprint_session_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    FakeBiodClient::Get()->SendEnrollScanDone(
        kFingerprint, biod::SCAN_RESULT_SUCCESS, true /* is_complete */,
        -1 /* percent_complete */);

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kQuickUnlockFingerprintRecord, 1);
  }

  void AuthenticateWithFingerprint() {
    FakeBiodClient::Get()->SendAuthScanDone(kFingerprint,
                                            biod::SCAN_RESULT_SUCCESS);
    base::RunLoop().RunUntilIdle();
  }

  base::TimeDelta GetExpirationTime() {
    int frequency = browser()->profile()->GetPrefs()->GetInteger(
        prefs::kQuickUnlockTimeout);
    return quick_unlock::PasswordConfirmationFrequencyToTimeDelta(
        static_cast<quick_unlock::PasswordConfirmationFrequency>(frequency));
  }

  void SetUpQuickUnlock() {
    // Get quick unlock storage and prepare for auth token manipulation.
    quick_unlock_storage_ = quick_unlock::QuickUnlockFactory::GetForAccountId(
        user_manager::StubAccountId());
    quick_unlock_storage_->SetClockForTesting(test_clock_.get());
  }

  void SetupTestClocks() {
    // Creates test clock to be injected into quick_unlock_storage.
    // Also creates test tick clock to simulate system sleep.
    // TickClock is paused when system is suspended so, clock and tick clock
    // can change by different amounts during time periods with system suspend.
    // This difference is important for fingerprint unlock policy so tests fake
    // both clock and tick clock, even though only clock is used by
    // quick unlock storage directly.
    base::Time now = base::Time::Now();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(now);
    // Creates test tick clock.
    base::TimeTicks now_ticks = base::TimeTicks::Now();
    test_tick_clock_ = std::make_unique<base::SimpleTestTickClock>();
    test_tick_clock_->SetNowTicks(now_ticks);
  }

  void MarkStrongAuth() { quick_unlock_storage_->MarkStrongAuth(); }

  bool HasStrongAuth() { return quick_unlock_storage_->HasStrongAuth(); }

  void AdvanceTime(base::TimeDelta time_change, base::TimeDelta sleep_time) {
    // System time is paused when system goes to sleep but real_time is not
    // so amount of time by which tick clock is advanced should not include
    // sleep time.
    ASSERT_GT(time_change, sleep_time);
    test_clock_->Advance(time_change);
    test_tick_clock_->Advance(time_change - sleep_time);
  }

 private:
  // Callback function for FakeBiodClient->StartEnrollSession.
  void OnStartSession(const dbus::ObjectPath& path) {
    fingerprint_session_started_ = true;
    if (fingerprint_session_callback_)
      std::move(fingerprint_session_callback_).Run();
  }

  bool fingerprint_session_started_ = false;

  base::OnceClosure fingerprint_session_callback_;

  QuickUnlockStorage* quick_unlock_storage_;

  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<base::SimpleTestTickClock> test_tick_clock_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintUnlockTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FingerprintUnlockTest, FingerprintNotTimedOutTest) {
  // Show lock screen and wait until it is shown.
  ScreenLockerTester tester;
  tester.Lock();

  // Mark strong auth, checks for strong auth.
  // The default is one day, so verify moving the last strong auth time back 12
  // hours(half of the expiration time) should not request strong auth.
  MarkStrongAuth();
  EXPECT_TRUE(HasStrongAuth());
  base::TimeDelta expiration_time = GetExpirationTime();
  AdvanceTime(expiration_time / 2, base::TimeDelta::FromSeconds(0));
  EXPECT_TRUE(HasStrongAuth());

  // Verify that fingerprint unlock is possible and the user can log in.
  AuthenticateWithFingerprint();
  EXPECT_FALSE(tester.IsLocked());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()
                   ->notify_lock_screen_dismissed_call_count());
}

IN_PROC_BROWSER_TEST_F(FingerprintUnlockTest, FingerprintTimedOutTest) {
  // Show lock screen and wait until it is shown.
  ScreenLockerTester tester;
  tester.Lock();

  // Mark strong auth, checks for strong auth.
  // The default is one day, so verify moving the last strong auth time back 12
  // hours(half of the expiration time) should not request strong auth.
  MarkStrongAuth();
  EXPECT_TRUE(HasStrongAuth());
  base::TimeDelta expiration_time = GetExpirationTime();
  AdvanceTime(expiration_time, base::TimeDelta::FromSeconds(0));
  EXPECT_FALSE(HasStrongAuth());

  // Verify that fingerprint unlock is not possible and the user cannot log in.
  AuthenticateWithFingerprint();
  EXPECT_TRUE(tester.IsLocked());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::LOCKED,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()
                   ->notify_lock_screen_dismissed_call_count());

  // Auth Error button should be visible as fingerprint has expired.
  EXPECT_TRUE(tester.IsAuthErrorBubbleShown());
}

IN_PROC_BROWSER_TEST_F(FingerprintUnlockTest, TimeoutIncludesSuspendedTime) {
  // Show lock screen and wait until it is shown.
  ScreenLockerTester tester;
  tester.Lock();

  // Mark strong auth, checks for strong auth.
  // The default is one day, so verify moving the last strong auth time back 12
  // hours(half of the expiration time) should not request strong auth.
  MarkStrongAuth();
  EXPECT_TRUE(HasStrongAuth());
  base::TimeDelta expiration_time = GetExpirationTime();
  AdvanceTime(expiration_time, expiration_time / 2);
  EXPECT_FALSE(HasStrongAuth());

  // Verify that fingerprint unlock is not possible and the user cannot log in.
  AuthenticateWithFingerprint();
  EXPECT_TRUE(tester.IsLocked());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::LOCKED,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()
                   ->notify_lock_screen_dismissed_call_count());

  // Auth Error button should be visible as fingerprint has expired.
  EXPECT_TRUE(tester.IsAuthErrorBubbleShown());
}

}  // namespace chromeos
