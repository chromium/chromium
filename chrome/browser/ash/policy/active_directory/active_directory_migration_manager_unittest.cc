// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/active_directory/active_directory_migration_manager.h"

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace {

constexpr base::TimeDelta kLongTimeDelta = base::Hours(25);
constexpr base::TimeDelta kShortTimeDelta = base::Hours(2);

}  // namespace

namespace policy {

class ActiveDirectoryMigrationManagerTest : public testing::Test {
 public:
  ActiveDirectoryMigrationManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(TestingBrowserProcess::GetGlobal()) {
    ash::SessionManagerClient::InitializeFake();
    session_manager_client_ = ash::FakeSessionManagerClient::Get();

    SetEnrollmentIdUploadedPref(/*value=*/false);
    SetChromadMigrationEnabledPref(/*value=*/false);
    SetLastMigrationAttemptTimePref(base::Time::Now() - kLongTimeDelta);
  }

  ~ActiveDirectoryMigrationManagerTest() override {
    migration_manager_->Shutdown();
    migration_manager_.reset();
    ash::SessionManagerClient::Shutdown();
  }

 protected:
  void ExpectStatus(base::RunLoop* run_loop,
                    bool expect_started,
                    bool expect_rescheduled,
                    bool started,
                    bool rescheduled) {
    EXPECT_EQ(expect_started, started);
    EXPECT_EQ(expect_rescheduled, rescheduled);
    run_loop->Quit();
  }

  // Sets expectations for an attempt to start the device migration.
  void ExpectAttemptToMigrate(base::RunLoop* run_loop,
                              bool expect_started,
                              bool expect_rescheduled) {
    migration_manager_->SetStatusCallbackForTesting(base::BindOnce(
        &ActiveDirectoryMigrationManagerTest::ExpectStatus,
        base::Unretained(this), run_loop, expect_started, expect_rescheduled));
  }

  void CreateMigrationManager() {
    migration_manager_ =
        std::make_unique<ActiveDirectoryMigrationManager>(local_state_.Get());
  }

  // Sets the value of `kEnrollmentIdUploadedOnChromad` pref.
  void SetEnrollmentIdUploadedPref(bool value) {
    local_state_.Get()->SetBoolean(prefs::kEnrollmentIdUploadedOnChromad,
                                   value);
  }

  // Sets the value of `kChromadToCloudMigrationEnabled` pref.
  void SetChromadMigrationEnabledPref(bool value) {
    local_state_.Get()->SetBoolean(ash::prefs::kChromadToCloudMigrationEnabled,
                                   value);
  }

  // Sets the value of `kLastChromadMigrationAttemptTime` pref.
  void SetLastMigrationAttemptTimePref(base::Time value) {
    local_state_.Get()->SetTime(prefs::kLastChromadMigrationAttemptTime, value);
  }

  session_manager::SessionManager session_manager_;
  raw_ptr<ash::FakeSessionManagerClient> session_manager_client_;
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_first_;
  base::RunLoop run_loop_following_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<ActiveDirectoryMigrationManager> migration_manager_;
};

// Starting with preconditions met: the manager triggers the migration.
TEST_F(ActiveDirectoryMigrationManagerTest, ConditionsAlreadyMetSucceeds) {
  SetEnrollmentIdUploadedPref(/*value=*/true);
  SetChromadMigrationEnabledPref(/*value=*/true);

  CreateMigrationManager();

  // Retry not scheduled, because the migration has already started.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/true,
                         /*expect_rescheduled=*/false);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 1);
}

// Login screen opened, but other conditions missing: the manager doesn't
// trigger the migration.
TEST_F(ActiveDirectoryMigrationManagerTest, LoginScreenOpenedFails) {
  CreateMigrationManager();

  // Retry scheduled, because the devices is on the login screen.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Retry not scheduled, because there is still one retry pending.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/false,
                         /*expect_rescheduled=*/false);

  session_manager_.NotifyLoginOrLockScreenVisible();
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);
}

// Login screen opened, and other conditions met: the manager triggers the
// migration.
TEST_F(ActiveDirectoryMigrationManagerTest, LoginScreenOpenedSucceeds) {
  // Starting with `kLastChromadMigrationAttemptTime` set to a short time ago,
  // to prevent the migration from starting during the initialization.
  SetEnrollmentIdUploadedPref(/*value=*/true);
  SetChromadMigrationEnabledPref(/*value=*/true);
  SetLastMigrationAttemptTimePref(base::Time::Now() - kShortTimeDelta);

  CreateMigrationManager();

  // Retry scheduled, because the device cannot start a powerwash yet.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Reverting this pref to a long time ago, to allow the migration to start.
  SetLastMigrationAttemptTimePref(base::Time::Now() - kLongTimeDelta);

  // Retry not scheduled, because the migration has already started.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/true,
                         /*expect_rescheduled=*/false);

  session_manager_.NotifyLoginOrLockScreenVisible();
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 1);
}

// EID uploaded, but other conditions missing: the manager doesn't trigger the
// migration.
TEST_F(ActiveDirectoryMigrationManagerTest, EnrollmentIdUploadedFails) {
  // Simulates user logged in.
  session_manager_.SessionStarted();
  CreateMigrationManager();

  // Retry not scheduled, because the user is logged in.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/false);

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  migration_manager_->Init();
  run_loop_first_.Run();

  // Retry not scheduled, because the user is logged in.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/false,
                         /*expect_rescheduled=*/false);

  SetEnrollmentIdUploadedPref(/*value=*/true);
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);
}

// EID uploaded, and other conditions met: the manager triggers the migration.
TEST_F(ActiveDirectoryMigrationManagerTest, EnrollmentIdUploadedSucceeds) {
  SetChromadMigrationEnabledPref(/*value=*/true);
  CreateMigrationManager();

  // Retry scheduled, because the device is on the login screen.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Retry not scheduled, because the migration has already started.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/true,
                         /*expect_rescheduled=*/false);

  SetEnrollmentIdUploadedPref(/*value=*/true);
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 1);
}

// Policy enabled, but other conditions missing: the manager doesn't trigger the
// migration.
TEST_F(ActiveDirectoryMigrationManagerTest, PolicyEnabledFails) {
  // Simulates user logged in.
  session_manager_.SessionStarted();
  CreateMigrationManager();

  // Retry not scheduled, because the user is logged in.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/false);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Retry not scheduled, because the user is logged in.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/false,
                         /*expect_rescheduled=*/false);

  SetChromadMigrationEnabledPref(/*value=*/true);
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);
}

// Policy enabled, and other conditions met: the manager triggers the migration.
TEST_F(ActiveDirectoryMigrationManagerTest, PolicyEnabledSucceeds) {
  SetEnrollmentIdUploadedPref(/*value=*/true);
  CreateMigrationManager();

  // Retry scheduled, because the device is on the login screen.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Retry not scheduled, because the migration has already started.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/true,
                         /*expect_rescheduled=*/false);

  SetChromadMigrationEnabledPref(/*value=*/true);
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 1);
}

// Policy disabled: the manager doesn't trigger the migration.
TEST_F(ActiveDirectoryMigrationManagerTest, PolicyDisabledFails) {
  // Starting with the policy enabled.
  SetChromadMigrationEnabledPref(/*value=*/true);
  CreateMigrationManager();

  // Retry scheduled, because the device is on the login screen.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Retry not scheduled, because there is still one retry pending.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/false,
                         /*expect_rescheduled=*/false);

  SetChromadMigrationEnabledPref(/*value=*/false);
  run_loop_following_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);
}

// Manager retries to start the migration, after the retry delay has passed.
TEST_F(ActiveDirectoryMigrationManagerTest, RetryRunAfterSomeTime) {
  CreateMigrationManager();

  // Retry scheduled, because the device is on the login screen.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();
  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);

  // Retry scheduled, because the device is on the login screen.
  ExpectAttemptToMigrate(&run_loop_following_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  run_loop_following_.Run();
  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);
}

// Manager has recently sent a powerwash request: migration is not started.
TEST_F(ActiveDirectoryMigrationManagerTest, PowerwashRecentlyRequestedFails) {
  SetEnrollmentIdUploadedPref(/*value=*/true);
  SetChromadMigrationEnabledPref(/*value=*/true);
  SetLastMigrationAttemptTimePref(base::Time::Now() - kShortTimeDelta);

  CreateMigrationManager();

  // Retry scheduled, because the device cannot start a powerwash yet.
  ExpectAttemptToMigrate(&run_loop_first_, /*expect_started=*/false,
                         /*expect_rescheduled=*/true);

  migration_manager_->Init();
  run_loop_first_.Run();

  EXPECT_EQ(session_manager_client_->start_device_wipe_call_count(), 0);
}

}  // namespace policy
