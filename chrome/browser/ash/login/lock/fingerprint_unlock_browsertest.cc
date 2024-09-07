// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/lock_screen.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {
namespace {

using QuickUnlockStorage = quick_unlock::QuickUnlockStorage;

constexpr char kFingerprint[] = "pinky";

// Simulate fingerprint authentication.
void AuthenticateAndCheckThroughHistogram(
    base::HistogramTester& histogram_tester,
    FakeBiodClient* biod) {
  biod::FingerprintMessage msg;
  msg.set_scan_result(biod::SCAN_RESULT_TOO_FAST);
  biod->SendAuthScanDone(kFingerprint, msg);
  msg.set_scan_result(biod::SCAN_RESULT_SUCCESS);
  biod->SendAuthScanDone(kFingerprint, msg);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples("Fingerprint.Auth.ScanResult"),
              ::testing::ElementsAre(
                  base::Bucket(static_cast<base::HistogramBase::Sample>(
                                   device::mojom::ScanResult::SUCCESS),
                               1),
                  base::Bucket(static_cast<base::HistogramBase::Sample>(
                                   device::mojom::ScanResult::TOO_FAST),
                               1)));
}

class FingerprintUnlockTest : public InProcessBrowserTest {
 public:
  FingerprintUnlockTest() = default;

  FingerprintUnlockTest(const FingerprintUnlockTest&) = delete;
  FingerprintUnlockTest& operator=(const FingerprintUnlockTest&) = delete;

  ~FingerprintUnlockTest() override = default;

  void SetUp() override {
    test_api_ = std::make_unique<quick_unlock::TestApi>(
        /*override_quick_unlock=*/true);
    test_api_->EnableFingerprintByPolicy(quick_unlock::Purpose::kUnlock);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  void SetUpInProcessBrowserTestFixture() override {
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    biod_ = FakeBiodClient::Get();
  }

  void EnrollFingerprint() {
    biod_->StartEnrollSession(
        "test-user", std::string(),
        base::BindOnce(&FingerprintUnlockTest::OnStartSession,
                       base::Unretained(this)));
    if (!fingerprint_session_started_) {
      base::RunLoop run_loop;
      fingerprint_session_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    biod_->SendEnrollScanDone(kFingerprint, biod::SCAN_RESULT_SUCCESS,
                              true /* is_complete */,
                              -1 /* percent_complete */);

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kQuickUnlockFingerprintRecord, 1);
  }

  void AuthenticateWithFingerprint() {
    biod::FingerprintMessage msg;
    msg.set_scan_result(biod::SCAN_RESULT_SUCCESS);
    biod_->SendAuthScanDone(kFingerprint, msg);
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
    // Sets clocks to Now() because a first strong auth is marked at session
    // initialization, with the default clock (before the test clock could be
    // set) and this strong auth mark is used by
    // ScreenLocker::update_fingerprint_state_timer_ as start time.
    base::Time now = base::Time::Now();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(now);
    // Creates test tick clock.
    base::TimeTicks now_ticks = base::TimeTicks::Now();
    test_tick_clock_ = std::make_unique<base::SimpleTestTickClock>();
    test_tick_clock_->SetNowTicks(now_ticks);
  }

  bool HasStrongAuth() { return quick_unlock_storage_->HasStrongAuth(); }

  void AdvanceTime(base::TimeDelta time_change, base::TimeDelta sleep_time) {
    // System time is paused when system goes to sleep but real_time is not
    // so amount of time by which tick clock is advanced should not include
    // sleep time.
    ASSERT_GE(time_change, sleep_time);

    // `ScreenLocker::update_fingerprint_state_timer_` is a WallClockTimer,
    // implementing PowerSuspendObserver.
    base::PowerMonitorDeviceSource::HandleSystemSuspending();
    test_clock_->Advance(time_change);
    test_tick_clock_->Advance(time_change - sleep_time);
    base::PowerMonitorDeviceSource::HandleSystemResumed();
  }

  void ShowLockScreenAndAdvanceTime(base::TimeDelta time_change,
                                    base::TimeDelta sleep_time) {
    ScreenLocker::SetClocksForTesting(test_clock_.get(),
                                      test_tick_clock_.get());

    // Show lock screen and wait until it is shown.
    ScreenLockerTester tester;
    tester.Lock();

    EXPECT_TRUE(HasStrongAuth());
    base::TimeDelta expiration_time = GetExpirationTime();
    AdvanceTime(time_change, sleep_time);

    bool fingerprint_available = time_change < expiration_time;

    LockScreen::TestApi lock_screen_test(LockScreen::Get());
    LockContentsViewTestApi lock_contents_test(
        lock_screen_test.contents_view());
    // Allow lock screen timer to be executed.
    base::RunLoop().RunUntilIdle();

    FingerprintState actual_state =
        lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
    FingerprintState expected_state =
        fingerprint_available ? FingerprintState::AVAILABLE_DEFAULT
                              : FingerprintState::DISABLED_FROM_TIMEOUT;
    EXPECT_EQ(actual_state, expected_state);
    EXPECT_EQ(fingerprint_available, HasStrongAuth());

    // Fingerprint unlock is possible iff we have strong auth. After
    // authentication attempt with fingerprint, the screen is locked if we
    // don't.
    bool screen_locked = !HasStrongAuth();
    // Verify whether or not fingerprint unlock is possible and if the user can
    // or cannot log in.
    AuthenticateWithFingerprint();
    EXPECT_EQ(screen_locked, tester.IsLocked());
    EXPECT_EQ(
        1,
        FakeSessionManagerClient::Get()->notify_lock_screen_shown_call_count());
    session_manager::SessionState expected_session_state =
        screen_locked ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
    session_manager::SessionState actual_session_state =
        session_manager::SessionManager::Get()->session_state();
    EXPECT_EQ(expected_session_state, actual_session_state);
    EXPECT_EQ(!screen_locked, FakeSessionManagerClient::Get()
                                  ->notify_lock_screen_dismissed_call_count());
  }

 protected:
  raw_ptr<FakeBiodClient, DanglingUntriaged> biod_;  // Non-owning pointer.
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<base::SimpleTestTickClock> test_tick_clock_;

 private:
  // Callback function for FakeBiodClient->StartEnrollSession.
  void OnStartSession(const dbus::ObjectPath& path) {
    fingerprint_session_started_ = true;
    if (fingerprint_session_callback_)
      std::move(fingerprint_session_callback_).Run();
  }

  bool fingerprint_session_started_ = false;

  base::OnceClosure fingerprint_session_callback_;

  raw_ptr<QuickUnlockStorage, DanglingUntriaged> quick_unlock_storage_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  std::unique_ptr<quick_unlock::TestApi> test_api_;
};

// Provides test clocks, quick unlock and an enrolled fingerprint to the tests.
class FingerprintUnlockEnrollTest : public FingerprintUnlockTest {
 public:
  FingerprintUnlockEnrollTest() = default;

  FingerprintUnlockEnrollTest(const FingerprintUnlockEnrollTest&) = delete;
  FingerprintUnlockEnrollTest& operator=(const FingerprintUnlockEnrollTest&) =
      delete;

  ~FingerprintUnlockEnrollTest() override = default;

  void SetUpOnMainThread() override {
    FingerprintUnlockTest::SetUpOnMainThread();
    SetupTestClocks();
    SetUpQuickUnlock();
    EnrollFingerprint();
  }
};

IN_PROC_BROWSER_TEST_F(FingerprintUnlockEnrollTest,
                       FingerprintNotTimedOutTest) {
  base::TimeDelta expiration_time = GetExpirationTime();
  ShowLockScreenAndAdvanceTime(expiration_time / 2, base::Seconds(0));
}

IN_PROC_BROWSER_TEST_F(FingerprintUnlockEnrollTest, FingerprintTimedOutTest) {
  base::TimeDelta expiration_time = GetExpirationTime();
  ShowLockScreenAndAdvanceTime(expiration_time, base::Seconds(0));
}

IN_PROC_BROWSER_TEST_F(FingerprintUnlockEnrollTest,
                       TimeoutIncludesSuspendedTime) {
  base::TimeDelta expiration_time = GetExpirationTime();
  ShowLockScreenAndAdvanceTime(expiration_time, expiration_time / 2);
}

IN_PROC_BROWSER_TEST_F(FingerprintUnlockTest, BiodFailsBeforeLockScreenReady) {
  ScreenLockerTester tester;
  tester.Lock();

  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());

  base::RunLoop().RunUntilIdle();

  // No fingerprint record is available yet.
  FingerprintState initial_state =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(initial_state, FingerprintState::UNAVAILABLE);

  // Simulate a biod late start, giving us fingerprint records.
  EnrollFingerprint();
  biod_->SendRestarted();
  biod_->SendStatusChanged(biod::BiometricsManagerStatus::INITIALIZED);
  base::RunLoop().RunUntilIdle();

  FingerprintState state_after_getting_records =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(state_after_getting_records, FingerprintState::AVAILABLE_DEFAULT);

  EXPECT_TRUE(tester.IsLocked());
  base::HistogramTester histogram_tester;
  // Note that we do not need to start an auth session since this should be
  // handled automatically by the lock screen when it receives a biod restart
  // signal.
  AuthenticateAndCheckThroughHistogram(histogram_tester, biod_);
  EXPECT_FALSE(tester.IsLocked());
}

// TODO(crbug.com/361782081): Fix and re-enable this test.
IN_PROC_BROWSER_TEST_F(FingerprintUnlockEnrollTest,
                       ExceedAttemptsAndBiodRestart) {
  ScreenLockerTester tester;
  biod::FingerprintMessage msg;

  tester.Lock();

  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());

  base::RunLoop().RunUntilIdle();

  for (int attempt = 0;
       attempt < quick_unlock::FingerprintStorage::kMaximumUnlockAttempts;
       attempt++) {
    FingerprintState state =
        lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
    EXPECT_EQ(state, FingerprintState::AVAILABLE_DEFAULT);
    // Simulate bad attempt.
    msg.set_scan_result(biod::SCAN_RESULT_TOO_FAST);
    biod_->SendAuthScanDone(kFingerprint, msg);
    base::RunLoop().RunUntilIdle();
  }

  FingerprintState state_after_attempts =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(state_after_attempts, FingerprintState::DISABLED_FROM_ATTEMPTS);

  // Emulate another biod restart giving us a different number of records so
  // `ScreenLocker::UpdateFingerprintStateForUser` can be triggered and so we
  // can check that the state indeed remains the same.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kQuickUnlockFingerprintRecord, 2);
  base::RunLoop().RunUntilIdle();

  // Check if we still have `FingerprintState::DISABLED_FROM_ATTEMPTS` status
  // after a restart.
  FingerprintState state_after_restart =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(state_after_restart, FingerprintState::DISABLED_FROM_ATTEMPTS);
}

IN_PROC_BROWSER_TEST_F(FingerprintUnlockEnrollTest,
                       BiodFailsAfterLockScreenReady) {
  ScreenLockerTester tester;
  tester.Lock();

  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());

  base::RunLoop().RunUntilIdle();

  FingerprintState initial_state =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(initial_state, FingerprintState::AVAILABLE_DEFAULT);

  // This corresponds to getting no record from biod service, although
  // fingerprint records were previously recorded for this user.
  biod_->DestroyAllRecords(base::DoNothing());
  biod_->SendRestarted();
  biod_->SendStatusChanged(biod::BiometricsManagerStatus::INITIALIZED);
  base::RunLoop().RunUntilIdle();

  FingerprintState state_after_bad_session =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(state_after_bad_session, FingerprintState::UNAVAILABLE);

  // Emulate another biod restart, giving a record this time.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kQuickUnlockFingerprintRecord, 1);

  FingerprintState state_after_restart =
      lock_contents_test.GetFingerPrintState(user_manager::StubAccountId());
  EXPECT_EQ(state_after_restart, FingerprintState::AVAILABLE_DEFAULT);
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, PRE_FingerprintRecordsGone) {
  // Pretend that user has a fingerprint enrolled. Number of enrolled
  // fingerprints is cached in the prefs. But the actual fingerprint records
  // are gone.
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord, 1);
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, FingerprintRecordsGone) {
  base::RunLoop().RunUntilIdle();
  Profile* profile = browser()->profile();
  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(prefs::kQuickUnlockFingerprintRecord), 0);
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, FingerprintScanResult) {
  FakeBiodClient* biod = FakeBiodClient::Get();

  // Simulate fingerprint enrollment.
  biod->StartEnrollSession(
      "test-user", std::string(),
      base::BindOnce([](const dbus::ObjectPath& result) {}));
  biod->SendEnrollScanDone(kFingerprint, biod::SCAN_RESULT_TOO_SLOW,
                           false /* is_complete */, -1 /* percent_complete */);
  biod->SendEnrollScanDone(kFingerprint, biod::SCAN_RESULT_SUCCESS,
                           true /* is_complete */, 100 /* percent_complete */);

  // Start auth session before authenticate.
  biod->StartAuthSession(base::BindOnce([](const dbus::ObjectPath& result) {}));

  base::HistogramTester histogram_tester;
  AuthenticateAndCheckThroughHistogram(histogram_tester, biod);
  EXPECT_THAT(histogram_tester.GetAllSamples("Fingerprint.Enroll.ScanResult"),
              ::testing::ElementsAre(
                  base::Bucket(static_cast<base::HistogramBase::Sample>(
                                   device::mojom::ScanResult::SUCCESS),
                               1),
                  base::Bucket(static_cast<base::HistogramBase::Sample>(
                                   device::mojom::ScanResult::TOO_SLOW),
                               1)));
}

constexpr char kFingerprintSuccessHistogramName[] =
    "Fingerprint.Unlock.AuthSuccessful";
constexpr char kFingerprintAttemptsCountBeforeSuccessHistogramName[] =
    "Fingerprint.Unlock.AttemptsCountBeforeSuccess";
constexpr char kFingerprintRecentAttemptsCountBeforeSuccessHistogramName[] =
    "Fingerprint.Unlock.RecentAttemptsCountBeforeSuccess";
constexpr char kFeatureUsageMetric[] = "ChromeOS.FeatureUsage.Fingerprint";

// Verifies that fingerprint auth success is recorded correctly.
IN_PROC_BROWSER_TEST_F(FingerprintUnlockEnrollTest, FeatureUsageMetrics) {
  ScreenLocker::SetClocksForTesting(test_clock_.get(), test_tick_clock_.get());

  // Show lock screen and wait until it is shown.
  ScreenLockerTester tester;
  tester.Lock();

  base::HistogramTester histogram_tester;
  biod::FingerprintMessage msg;

  EXPECT_TRUE(HasStrongAuth());
  msg.set_scan_result(biod::SCAN_RESULT_TOO_FAST);
  biod_->SendAuthScanDone(kFingerprint, msg);
  msg.set_scan_result(biod::SCAN_RESULT_SUCCESS);
  biod_->SendAuthScanDone(kFingerprint, msg);
  tester.WaitForUnlock();
  histogram_tester.ExpectBucketCount(kFingerprintSuccessHistogramName,
                                     /*success=*/1, 1);
  histogram_tester.ExpectBucketCount(
      "Fingerprint.Unlock.Result",
      static_cast<int>(quick_unlock::FingerprintUnlockResult::kSuccess), 1);
  histogram_tester.ExpectTotalCount(
      kFingerprintAttemptsCountBeforeSuccessHistogramName, 1);
  histogram_tester.ExpectTotalCount(
      kFingerprintRecentAttemptsCountBeforeSuccessHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kFeatureUsageMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
  histogram_tester.ExpectBucketCount(
      kFeatureUsageMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure),
      1);
  histogram_tester.ExpectBucketCount(
      kFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);
  histogram_tester.ExpectBucketCount(
      kFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      1);
  histogram_tester.ExpectBucketCount(
      kFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);

  histogram_tester.ExpectBucketCount(kFingerprintSuccessHistogramName,
                                     /*success=*/0, 1);
  histogram_tester.ExpectBucketCount(
      "Fingerprint.Unlock.Result",
      static_cast<int>(quick_unlock::FingerprintUnlockResult::kMatchFailed), 1);
}

}  // namespace
}  // namespace ash
