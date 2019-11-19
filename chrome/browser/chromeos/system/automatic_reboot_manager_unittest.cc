// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::ReturnPointee;

namespace chromeos {
namespace system {

namespace {

// Provides a mock device uptime that follows the mock time maintained by the
// given |mock_time_task_runner| with a configurable offset. The mock uptime can
// also be written to |uptime_file_|, thusly allowing to mock /proc/uptime.
class MockUptimeProvider {
 public:
  // The |mock_time_task_runner| must outlive this object. MockUptimeProvider
  // will not keep a reference to it, so that it can be owned by the very same
  // |mock_time_task_runner| instance.
  explicit MockUptimeProvider(
      base::TestMockTimeTaskRunner* mock_time_task_runner);

  void WriteUptimeToFile();

  // Adjusts the offset so that the current mock uptime will be |uptime|.
  void SetUptime(const base::TimeDelta& uptime);

  void set_uptime_file_path(const base::FilePath& uptime_file_path) {
    uptime_file_path_ = uptime_file_path;
  }

  base::TimeDelta uptime() const {
    return mock_time_task_runner_->NowTicks() - base::TimeTicks() +
           uptime_offset_;
  }

 private:
  base::TestMockTimeTaskRunner* mock_time_task_runner_;

  base::FilePath uptime_file_path_;
  base::TimeDelta uptime_offset_;

  DISALLOW_COPY_AND_ASSIGN(MockUptimeProvider);
};

class TestAutomaticRebootManagerTaskRunner
    : public base::TestMockTimeTaskRunner {
 public:
  TestAutomaticRebootManagerTaskRunner();

  MockUptimeProvider* uptime_provider() const {
    return uptime_provider_.get();
  }

 private:
  ~TestAutomaticRebootManagerTaskRunner() override;

  // base::TestMockTimeTaskRunner:
  void OnBeforeSelectingTask() override;
  void OnAfterTimePassed() override;
  void OnAfterTaskRun() override;

  std::unique_ptr<MockUptimeProvider> uptime_provider_;

  DISALLOW_COPY_AND_ASSIGN(TestAutomaticRebootManagerTaskRunner);
};

class MockAutomaticRebootManagerObserver
    : public AutomaticRebootManagerObserver {
 public:
  MockAutomaticRebootManagerObserver();
  ~MockAutomaticRebootManagerObserver() override;

  void Init(AutomaticRebootManager* automatic_reboot_manger);

  // AutomaticRebootManagerObserver:
  MOCK_METHOD1(OnRebootRequested, void(Reason));
  MOCK_METHOD0(WillDestroyAutomaticRebootManager, void());

 private:
  void StopObserving();

  AutomaticRebootManager* automatic_reboot_manger_;

  DISALLOW_COPY_AND_ASSIGN(MockAutomaticRebootManagerObserver);
};

}  // namespace

class AutomaticRebootManagerBasicTest : public testing::Test {
 protected:
  AutomaticRebootManagerBasicTest();
  ~AutomaticRebootManagerBasicTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void SetUpdateRebootNeededUptime(const base::TimeDelta& uptime);
  void SetRebootAfterUpdate(bool reboot_after_update, bool expect_reboot);
  void SetUptimeLimit(const base::TimeDelta& limit, bool expect_reboot);
  void NotifyUpdateRebootNeeded();
  void NotifyResumed(bool expect_reboot);
  void NotifyTerminating(bool expect_reboot);

  void FastForwardBy(const base::TimeDelta& delta, bool expect_reboot);
  void FastForwardUntilNoTasksRemain(bool expect_reboot);

  void ExpectRebootRequest(AutomaticRebootManagerObserver::Reason reason);
  void ExpectNoRebootRequest();

  void CreateAutomaticRebootManager(bool expect_reboot);

  bool ReadUpdateRebootNeededUptimeFromFile(base::TimeDelta* uptime);
  void VerifyRebootRequested(AutomaticRebootManagerObserver::Reason reason);
  void VerifyNoRebootRequested() const;
  void VerifyLoginScreenIdleTimerIsStopped() const;
  void VerifyNoGracePeriod() const;
  void VerifyGracePeriod(const base::TimeDelta& start_uptime) const;

  // Sets the status of |update_engine_client_| to NEED_REBOOT for tests.
  void SetUpdateStatusNeedReboot();

  void LogIn();

  MockUptimeProvider* uptime_provider() const {
    return task_runner_->uptime_provider();
  }

  bool is_kiosk_session() const {
    return is_logged_in_as_kiosk_app_ || is_logged_in_as_arc_kiosk_app_;
  }

  void VerifyTimerIsStopped(const base::OneShotTimer* timer) const;
  void VerifyTimerIsRunning(const base::OneShotTimer* timer,
                            const base::TimeDelta& delay) const;
  void VerifyLoginScreenIdleTimerIsRunning() const;

  bool is_user_logged_in_ = false;
  bool is_logged_in_as_kiosk_app_ = false;
  bool is_logged_in_as_arc_kiosk_app_ = false;

  // The uptime is read in the blocking thread pool and then processed on the
  // UI thread. This causes the UI thread to start processing the uptime when it
  // has increased by a small offset already. The offset is calculated and
  // stored in |uptime_processing_delay_| so that tests can accurately determine
  // the uptime seen by the UI thread.
  base::TimeDelta uptime_processing_delay_;
  base::TimeDelta update_reboot_needed_uptime_;
  base::TimeDelta uptime_limit_;

  scoped_refptr<TestAutomaticRebootManagerTaskRunner> task_runner_;

  MockAutomaticRebootManagerObserver automatic_reboot_manager_observer_;
  std::unique_ptr<AutomaticRebootManager> automatic_reboot_manager_;

  base::ScopedTempDir temp_dir_;
  base::FilePath update_reboot_needed_uptime_file_;

  bool reboot_after_update_ = false;

  base::test::TaskEnvironment task_environment_;
  base::ScopedClosureRunner reset_main_thread_task_runner_;

  TestingPrefServiceSimple local_state_;
  MockUserManager* mock_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  session_manager::SessionManager session_manager_;

  FakeUpdateEngineClient* update_engine_client_ = nullptr;  // Not owned.

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomaticRebootManagerBasicTest);
};

enum AutomaticRebootManagerTestScenario {
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN,
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION,
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_ARC_KIOSK_APP_SESSION,
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION,
};

// This class runs each test case three times:
// * once while the login screen is being shown
// * once while a kiosk app session is in progress
// * once while a non-kiosk-app session is in progress
class AutomaticRebootManagerTest
    : public AutomaticRebootManagerBasicTest,
      public testing::WithParamInterface<AutomaticRebootManagerTestScenario> {
 protected:
  AutomaticRebootManagerTest();
  virtual ~AutomaticRebootManagerTest();
};

void SaveUptimeToFile(const base::FilePath& path,
                      const base::TimeDelta& uptime) {
  if (path.empty() || uptime.is_zero())
    return;

  const std::string uptime_seconds = base::NumberToString(uptime.InSecondsF());
  ASSERT_EQ(static_cast<int>(uptime_seconds.size()),
            base::WriteFile(path, uptime_seconds.c_str(),
                            uptime_seconds.size()));
}

MockUptimeProvider::MockUptimeProvider(
    base::TestMockTimeTaskRunner* mock_time_task_runner)
    : mock_time_task_runner_(mock_time_task_runner) {
}

void MockUptimeProvider::WriteUptimeToFile() {
  SaveUptimeToFile(uptime_file_path_, uptime());
}

void MockUptimeProvider::SetUptime(const base::TimeDelta& uptime) {
  uptime_offset_ =
      uptime - (mock_time_task_runner_->NowTicks() - base::TimeTicks());
  WriteUptimeToFile();
}

TestAutomaticRebootManagerTaskRunner::TestAutomaticRebootManagerTaskRunner()
    : uptime_provider_(new MockUptimeProvider(this)) {
}

TestAutomaticRebootManagerTaskRunner::~TestAutomaticRebootManagerTaskRunner() {
}

void TestAutomaticRebootManagerTaskRunner::OnBeforeSelectingTask() {
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

void TestAutomaticRebootManagerTaskRunner::OnAfterTimePassed() {
  uptime_provider_->WriteUptimeToFile();
}

void TestAutomaticRebootManagerTaskRunner::OnAfterTaskRun() {
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

MockAutomaticRebootManagerObserver::MockAutomaticRebootManagerObserver()
    : automatic_reboot_manger_(nullptr) {
  ON_CALL(*this, WillDestroyAutomaticRebootManager())
      .WillByDefault(
          Invoke(this,
                 &MockAutomaticRebootManagerObserver::StopObserving));
}

void MockAutomaticRebootManagerObserver::Init(
    AutomaticRebootManager* automatic_reboot_manger) {
  EXPECT_FALSE(automatic_reboot_manger_);
  automatic_reboot_manger_ = automatic_reboot_manger;
  automatic_reboot_manger_->AddObserver(this);
}

MockAutomaticRebootManagerObserver::~MockAutomaticRebootManagerObserver() {
  if (automatic_reboot_manger_)
    automatic_reboot_manger_->RemoveObserver(this);
}

void MockAutomaticRebootManagerObserver::StopObserving() {
  ASSERT_TRUE(automatic_reboot_manger_);
  automatic_reboot_manger_->RemoveObserver(this);
  automatic_reboot_manger_ = nullptr;
}

AutomaticRebootManagerBasicTest::AutomaticRebootManagerBasicTest()
    : task_runner_(new TestAutomaticRebootManagerTaskRunner),
      reset_main_thread_task_runner_(
          base::ThreadTaskRunnerHandle::OverrideForTesting(task_runner_)),
      mock_user_manager_(new MockUserManager),
      user_manager_enabler_(base::WrapUnique(mock_user_manager_)) {}

AutomaticRebootManagerBasicTest::~AutomaticRebootManagerBasicTest() {
}

void AutomaticRebootManagerBasicTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath& temp_dir = temp_dir_.GetPath();
  const base::FilePath uptime_file = temp_dir.Append("uptime");
  uptime_provider()->set_uptime_file_path(uptime_file);
  ASSERT_EQ(0, base::WriteFile(uptime_file, NULL, 0));
  update_reboot_needed_uptime_file_ =
      temp_dir.Append("update_reboot_needed_uptime");
  ASSERT_EQ(0, base::WriteFile(update_reboot_needed_uptime_file_, NULL, 0));
  ASSERT_TRUE(base::PathService::Override(FILE_UPTIME, uptime_file));
  ASSERT_TRUE(base::PathService::Override(FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                                          update_reboot_needed_uptime_file_));

  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  AutomaticRebootManager::RegisterPrefs(local_state_.registry());

  update_engine_client_ = new FakeUpdateEngineClient;
  DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
      std::unique_ptr<UpdateEngineClient>(update_engine_client_));
  PowerManagerClient::InitializeFake();

  EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
     .WillRepeatedly(ReturnPointee(&is_user_logged_in_));
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
     .WillRepeatedly(ReturnPointee(&is_logged_in_as_kiosk_app_));
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsArcKioskApp())
      .WillRepeatedly(ReturnPointee(&is_logged_in_as_arc_kiosk_app_));
}

void AutomaticRebootManagerBasicTest::TearDown() {
  if (automatic_reboot_manager_) {
    Mock::VerifyAndClearExpectations(&automatic_reboot_manager_observer_);
    EXPECT_CALL(automatic_reboot_manager_observer_,
                WillDestroyAutomaticRebootManager()).Times(1);
    EXPECT_CALL(automatic_reboot_manager_observer_,
                OnRebootRequested(_)).Times(0);
    // Let the AutomaticRebootManager, if any, unregister itself as an observer
    // of several subsystems.
    automatic_reboot_manager_.reset();
    task_runner_->RunUntilIdle();
  }

  PowerManagerClient::Shutdown();
  DBusThreadManager::Shutdown();
  TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
}

void AutomaticRebootManagerBasicTest::SetUpdateRebootNeededUptime(
    const base::TimeDelta& uptime) {
  update_reboot_needed_uptime_ = uptime;
  SaveUptimeToFile(update_reboot_needed_uptime_file_, uptime);
}

void AutomaticRebootManagerBasicTest::SetRebootAfterUpdate(
    bool reboot_after_update,
    bool expect_reboot) {
  reboot_after_update_ = reboot_after_update;
  local_state_.SetManagedPref(
      prefs::kRebootAfterUpdate,
      std::make_unique<base::Value>(reboot_after_update));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::SetUptimeLimit(
    const base::TimeDelta& limit,
    bool expect_reboot) {
  uptime_limit_ = limit;
  if (limit.is_zero()) {
    local_state_.RemoveManagedPref(prefs::kUptimeLimit);
  } else {
    local_state_.SetManagedPref(
        prefs::kUptimeLimit,
        std::make_unique<base::Value>(static_cast<int>(limit.InSeconds())));
  }
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyUpdateRebootNeeded() {
  SetUpdateStatusNeedReboot();
  automatic_reboot_manager_->UpdateStatusChanged(
      update_engine_client_->GetLastStatus());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyResumed(bool expect_reboot) {
  automatic_reboot_manager_->SuspendDone(base::TimeDelta::FromHours(1));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyTerminating(bool expect_reboot) {
  automatic_reboot_manager_->Observe(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::Source<AutomaticRebootManagerBasicTest>(this),
      content::NotificationService::NoDetails());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::FastForwardBy(
    const base::TimeDelta& delta,
    bool expect_reboot) {
  task_runner_->FastForwardBy(delta);
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::FastForwardUntilNoTasksRemain(
    bool expect_reboot) {
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::ExpectRebootRequest(
    AutomaticRebootManagerObserver::Reason reason) {
  Mock::VerifyAndClearExpectations(&automatic_reboot_manager_observer_);
  EXPECT_CALL(automatic_reboot_manager_observer_,
              WillDestroyAutomaticRebootManager()).Times(0);
  EXPECT_CALL(automatic_reboot_manager_observer_,
              OnRebootRequested(_)).Times(0);
  EXPECT_CALL(automatic_reboot_manager_observer_,
              OnRebootRequested(reason)).Times(1);
}

void AutomaticRebootManagerBasicTest::ExpectNoRebootRequest() {
  Mock::VerifyAndClearExpectations(&automatic_reboot_manager_observer_);
  EXPECT_CALL(automatic_reboot_manager_observer_,
              WillDestroyAutomaticRebootManager()).Times(0);
  EXPECT_CALL(automatic_reboot_manager_observer_,
              OnRebootRequested(_)).Times(0);
}

void AutomaticRebootManagerBasicTest::CreateAutomaticRebootManager(
    bool expect_reboot) {
  automatic_reboot_manager_ = std::make_unique<AutomaticRebootManager>(
      task_runner_->GetMockTickClock());
  automatic_reboot_manager_observer_.Init(automatic_reboot_manager_.get());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(expect_reboot ? 1 : 0,
            FakePowerManagerClient::Get()->num_request_restart_calls());

  uptime_processing_delay_ =
      base::TimeTicks() -
      automatic_reboot_manager_->boot_time_.value_or(base::TimeTicks()) -
      uptime_provider()->uptime();
  EXPECT_GE(uptime_processing_delay_, base::TimeDelta());
  EXPECT_LE(uptime_processing_delay_, base::TimeDelta::FromSeconds(1));

  if (is_user_logged_in_ || expect_reboot)
    VerifyLoginScreenIdleTimerIsStopped();
  else
    VerifyLoginScreenIdleTimerIsRunning();
}

bool AutomaticRebootManagerBasicTest::ReadUpdateRebootNeededUptimeFromFile(
    base::TimeDelta* uptime) {
  std::string contents;
  if (!base::ReadFileToString(update_reboot_needed_uptime_file_, &contents)) {
    return false;
  }
  double seconds;
  if (!base::StringToDouble(contents.substr(0, contents.find(' ')), &seconds) ||
      seconds < 0.0) {
    return false;
  }
  *uptime = base::TimeDelta::FromMilliseconds(seconds * 1000.0);
  return true;
}

void AutomaticRebootManagerBasicTest::VerifyRebootRequested(
    AutomaticRebootManagerObserver::Reason reason) {
  EXPECT_TRUE(automatic_reboot_manager_->reboot_requested());
  EXPECT_EQ(reason, automatic_reboot_manager_->reboot_reason());
}

void AutomaticRebootManagerBasicTest::VerifyNoRebootRequested() const {
  EXPECT_FALSE(automatic_reboot_manager_->reboot_requested());
}

void AutomaticRebootManagerBasicTest::
    VerifyLoginScreenIdleTimerIsStopped() const {
  VerifyTimerIsStopped(
      automatic_reboot_manager_->login_screen_idle_timer_.get());
}

void AutomaticRebootManagerBasicTest::VerifyNoGracePeriod() const {
  EXPECT_FALSE(automatic_reboot_manager_->reboot_requested_);
  VerifyTimerIsStopped(automatic_reboot_manager_->grace_start_timer_.get());
  VerifyTimerIsStopped(automatic_reboot_manager_->grace_end_timer_.get());
}

void AutomaticRebootManagerBasicTest::VerifyGracePeriod(
    const base::TimeDelta& start_uptime) const {
  const base::TimeDelta start =
      start_uptime - uptime_provider()->uptime() - uptime_processing_delay_;
  const base::TimeDelta end = start + base::TimeDelta::FromHours(24);
  if (start <= base::TimeDelta()) {
    EXPECT_TRUE(automatic_reboot_manager_->reboot_requested_);
    VerifyTimerIsStopped(automatic_reboot_manager_->grace_start_timer_.get());
    VerifyTimerIsRunning(automatic_reboot_manager_->grace_end_timer_.get(),
                         end);
  } else {
    EXPECT_FALSE(automatic_reboot_manager_->reboot_requested_);
    VerifyTimerIsRunning(automatic_reboot_manager_->grace_start_timer_.get(),
                         start);
    VerifyTimerIsRunning(automatic_reboot_manager_->grace_end_timer_.get(),
                         end);
  }
}

void AutomaticRebootManagerBasicTest::SetUpdateStatusNeedReboot() {
  update_engine::StatusResult client_status;
  client_status.set_current_operation(
      update_engine::Operation::UPDATED_NEED_REBOOT);
  update_engine_client_->set_default_status(client_status);
}

void AutomaticRebootManagerBasicTest::LogIn() {
  is_user_logged_in_ = true;

  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("email", "123456");
  session_manager_.CreateSession(
      account_id,
      chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting("email"), true);
  session_manager_.SessionStarted();
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
}

void AutomaticRebootManagerBasicTest::VerifyTimerIsStopped(
    const base::OneShotTimer* timer) const {
  if (timer)
    EXPECT_FALSE(timer->IsRunning());
}

void AutomaticRebootManagerBasicTest::VerifyTimerIsRunning(
    const base::OneShotTimer* timer,
    const base::TimeDelta& delay) const {
  ASSERT_TRUE(timer);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(delay.ToInternalValue(),
            timer->GetCurrentDelay().ToInternalValue());
}

void AutomaticRebootManagerBasicTest::
    VerifyLoginScreenIdleTimerIsRunning() const {
  VerifyTimerIsRunning(
      automatic_reboot_manager_->login_screen_idle_timer_.get(),
      base::TimeDelta::FromSeconds(60));
}

AutomaticRebootManagerTest::AutomaticRebootManagerTest() {
  switch (GetParam()) {
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN:
      is_user_logged_in_ = false;
      is_logged_in_as_kiosk_app_ = false;
      is_logged_in_as_arc_kiosk_app_ = false;
      session_manager_.SetSessionState(
          session_manager::SessionState::LOGIN_PRIMARY);
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION:
      is_logged_in_as_kiosk_app_ = true;
      is_logged_in_as_arc_kiosk_app_ = false;
      LogIn();
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_ARC_KIOSK_APP_SESSION:
      is_logged_in_as_kiosk_app_ = false;
      is_logged_in_as_arc_kiosk_app_ = true;
      LogIn();
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION:
      is_logged_in_as_kiosk_app_ = false;
      is_logged_in_as_arc_kiosk_app_ = false;
      LogIn();
      break;
  }
}

AutomaticRebootManagerTest::~AutomaticRebootManagerTest() {
}

// Chrome is showing the login screen. The current uptime is 12 hours.
// Verifies that the idle timer is running. Further verifies that when a kiosk
// app session begins, the idle timer is stopped.
TEST_F(AutomaticRebootManagerBasicTest, LoginStopsIdleTimer) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested, the device does not reboot immediately
  // and the login screen idle timer is started.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that a kiosk app session has been started.
  is_logged_in_as_kiosk_app_ = true;
  LogIn();

  // Verify that the login screen idle timer is stopped.
  VerifyLoginScreenIdleTimerIsStopped();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is showing the login screen. The current uptime is 12 hours.
// Verifies that the idle timer is running. Further verifies that when a
// non-kiosk-app session begins, the idle timer is stopped.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskLoginStopsIdleTimer) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested, the device does not reboot immediately
  // and the login screen idle timer is started.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that a non-kiosk-app session has been started.
  LogIn();

  // Verify that the login screen idle timer is stopped.
  VerifyLoginScreenIdleTimerIsStopped();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is showing the login screen. The uptime limit is 6 hours. The current
// uptime is 12 hours.
// Verifies that user activity prevents the device from rebooting. Further
// verifies that when user activity ceases, the devices reboots.
TEST_F(AutomaticRebootManagerBasicTest, UserActivityResetsIdleTimer) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested, the device does not reboot immediately
  // and the login screen idle timer is started.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 25 minutes while simulating user activity every
  // 50 seconds.
  for (int i = 0; i < 30; ++i) {
    // Fast forward uptime by 50 seconds. Verify that the device does not reboot
    // immediately.
    FastForwardBy(base::TimeDelta::FromSeconds(50), false);

    // Simulate user activity.
    automatic_reboot_manager_->OnUserActivity(NULL);
  }

  // Fast forward the uptime by 60 seconds without simulating user activity.
  // Verify that the device reboots immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(60), true);
}

// Chrome is running a kiosk app session. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, ResumeNoPolicy) {
  is_logged_in_as_kiosk_app_ = true;
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  NotifyResumed(false);

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a non-kiosk-app session. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeAppNoPolicy) {
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  NotifyResumed(false);

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 24 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, ResumeBeforeGracePeriod) {
  is_logged_in_as_kiosk_app_ = true;
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  NotifyResumed(false);

  // Verify a reboot is requested and the device reboots eventually.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 24 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeBeforeGracePeriod) {
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  NotifyResumed(false);

  // Verify that a reboot is requested eventually but the device never reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 6 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it immediately
// reboots.
TEST_F(AutomaticRebootManagerBasicTest, ResumeInGracePeriod) {
  is_logged_in_as_kiosk_app_ = true;
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device reboots immediately.
  NotifyResumed(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 6 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeInGracePeriod) {
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 6 hours. The
// current uptime is 29 hours 30 minutes.
// Verifies that when the device is suspended and then resumes, it immediately
// reboots.
TEST_F(AutomaticRebootManagerBasicTest, ResumeAfterGracePeriod) {
  is_logged_in_as_kiosk_app_ = true;
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(29) +
                          base::TimeDelta::FromMinutes(30));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device reboots immediately.
  NotifyResumed(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 6 hours. The
// current uptime is 29 hours 30 minutes.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeAfterGracePeriod) {
  LogIn();
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(29) +
                          base::TimeDelta::FromMinutes(30));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  NotifyResumed(false);

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is 10 days.
// Verifies that when the browser terminates, the device does not immediately
// reboot.
TEST_P(AutomaticRebootManagerTest, TerminateNoPolicy) {
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the browser is terminating. Verify that no reboot is requested
  // and the device does not reboot immediately.
  NotifyTerminating(false);

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 12 hours.
// Verifies that when the browser terminates, it does not immediately reboot.
TEST_P(AutomaticRebootManagerTest, TerminateBeforeGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the browser is terminating. Verify that no reboot is requested
  // and the device does not reboot immediately.
  NotifyTerminating(false);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 6 hours. The current uptime is
// 12 hours.
// Verifies that when the browser terminates, the device immediately reboots if
// a kiosk app session is in progress.
TEST_P(AutomaticRebootManagerTest, TerminateInGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the browser is terminating. Verify that the device immediately
  // reboots if a kiosk app session is in progress.
  NotifyTerminating(is_kiosk_session());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when the uptime limit is set to 24 hours, no reboot occurs and
// a grace period is scheduled to begin after 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, BeforeUptimeLimitGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when the uptime limit is set to 6 hours, a reboot is requested
// and a grace period is started that will end after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, InUptimeLimitGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The current uptime is 10 days.
// Verifies that when the uptime limit is set to 6 hours, the device reboots
// immediately if no non-kiosk-app-session is in progress because the grace
// period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, AfterUptimeLimitGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that a reboot is requested and unless a
  // non-kiosk-app session is in progress, the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6),
                 !is_user_logged_in_ || is_kiosk_session());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 6 hours.
// Verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffBeforeGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 1 hour. Verify that no reboot is requested and
  // the device does not reboot immediately.
  FastForwardBy(base::TimeDelta::FromHours(1), false);

  // Remove the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 24 hours.
// Verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffInGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(24));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Remove the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 6 hours.
// Verifies that when the uptime limit is extended to 24 hours, the grace period
// is rescheduled to start further in the future.
TEST_P(AutomaticRebootManagerTest, ExtendUptimeLimitBeforeGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that no reboot is requested
  // and the device does not reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Extend the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that the grace period has been rescheduled to start further in the
  // future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 18 hours.
// Verifies that when the uptime limit is extended to 24 hours, the grace period
// is rescheduled to start in the future.
TEST_P(AutomaticRebootManagerTest, ExtendUptimeLimitInGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(18));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(12), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Extend the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that the grace period has been rescheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested again eventually and unless a
  // non-kiosk-app session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 18 hours. The current uptime is
// 12 hours.
// Verifies that when the uptime limit is shortened to 6 hours, the grace period
// is rescheduled to have already started.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitBeforeToInGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(18), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that no reboot is requested
  // and the device does not reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Shorten the uptime limit. Verify that a reboot is requested but the device
  // does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that the grace period has been rescheduled and has started already.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 36 hours.
// Verifies that when the uptime limit is shortened to 18 hours, the grace
// period is rescheduled to have started earlier.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitInToInGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(36));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Shorten the uptime limit. Verify that a reboot is requested again but the
  // device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(18), false);

  // Verify that the grace period has been rescheduled to have started earlier.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 36 hours.
// Verifies that when the uptime limit is shortened to 6 hours, the device
// reboots immediately if no non-kiosk-app session is in progress because the
// grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitInToAfterGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(36));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Shorten the uptime limit. Verify that a reboot is requested again and
  // unless a non-kiosk-app session is in progress, the the device immediately
  // reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6),
                 !is_user_logged_in_ || is_kiosk_session());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when an update is applied, the current uptime is persisted as
// the time at which a reboot became necessary. Further verifies that when the
// policy to automatically reboot after an update is not enabled, no reboot
// occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, UpdateNoPolicy) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when an update is applied, the current uptime is persisted as
// the time at which a reboot became necessary. Further verifies that when the
// policy to automatically reboot after an update is enabled, a reboot is
// requested and a grace period is started that will end 24 hours from now.
TEST_P(AutomaticRebootManagerTest, Update) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when Chrome is notified twice that an update has been applied,
// the second notification is ignored and the uptime at which it occurred does
// not get persisted as the time at which an update became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateAfterUpdate) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the previously persisted time at which a reboot became
  // necessary has not been overwritten.
  base::TimeDelta new_update_reboot_needed_uptime;
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &new_update_reboot_needed_uptime));
  EXPECT_EQ(update_reboot_needed_uptime_, new_update_reboot_needed_uptime);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The current uptime is 10 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, no reboot occurs and a grace period is scheduled to begin after the
// minimum of 1 hour of uptime. Further verifies that when an update is applied,
// the current uptime is persisted as the time at which a reboot became
// necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeMinimumUptime) {
  uptime_provider()->SetUptime(base::TimeDelta::FromMinutes(10));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has been scheduled to begin in the future.
  VerifyGracePeriod(base::TimeDelta::FromHours(1));

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, PolicyAfterUpdateInGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Fast forward the uptime to 12 hours. Verify that no reboot is requested and
  // the device does not reboot immediately.
  FastForwardBy(base::TimeDelta::FromHours(6), false);

  // Simulate user activity.
  automatic_reboot_manager_->OnUserActivity(NULL);

  // Enable automatic reboot after an update has been applied. Verify that a
  // reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  SetRebootAfterUpdate(true, false);

  // Verify that a grace period has started.
  VerifyGracePeriod(base::TimeDelta::FromHours(6) + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is
// enabled, the device reboots immediately if no non-kiosk-app session is in
// progress because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, PolicyAfterUpdateAfterGracePeriod) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(6));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Fast forward the uptime to 12 hours. Verify that no reboot is requested and
  // the device does not reboot immediately.
  FastForwardBy(base::TimeDelta::FromDays(10) - base::TimeDelta::FromHours(6),
                false);

  // Simulate user activity.
  automatic_reboot_manager_->OnUserActivity(NULL);

  // Enable automatic rebooting after an update has been applied. Verify that
  // a reboot is requested and unless a non-kiosk-app session is in progress,
  // the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  SetRebootAfterUpdate(true, !is_user_logged_in_ || is_kiosk_session());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The policy to
// automatically reboot after an update is enabled. The current uptime is
// 6 hours 20 seconds.
// Verifies that when the policy to automatically reboot after an update is
// disabled, the reboot request and grace period are removed.
TEST_P(AutomaticRebootManagerTest, PolicyOffAfterUpdate) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(6));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  NotifyUpdateRebootNeeded();

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_provider()->uptime() + uptime_processing_delay_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Disable automatic rebooting after an update has been applied. Verify that
  // no reboot is requested and the device does not reboot immediately.
  SetRebootAfterUpdate(false, false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is not available.
// Verifies that even if an uptime limit is set, the policy to automatically
// reboot after an update is enabled and an update has been applied, no reboot
// occurs and no grace period is scheduled. Further verifies that no time is
// persisted as the time at which a reboot became necessary.
TEST_P(AutomaticRebootManagerTest, NoUptime) {
  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Enable automatic rebooting after an update has been applied. Verify that
  // no reboot is requested and the device does not reboot immediately.
  SetRebootAfterUpdate(true, false);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that no time is persisted as the time at which a reboot became
  // necessary.
  EXPECT_FALSE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The policy to automatically reboot after an update is
// enabled. The current uptime is 12 hours.
// Verifies that when an uptime limit of 6 hours is set, the availability of an
// update does not cause the grace period to be rescheduled. Further verifies
// that the current uptime is persisted as the time at which a reboot became
// necessary.
TEST_P(AutomaticRebootManagerTest, UptimeLimitBeforeUpdate) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested again but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that the grace period has not been rescheduled.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The policy to automatically reboot after an update is
// enabled. The current uptime is 12 hours.
// Verifies that when an uptime limit of 24 hours is set, the availability of an
// update causes the grace period to be rescheduled so that it ends 24 hours
// from now. Further verifies that the current uptime is persisted as the time
// at which a reboot became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeUptimeLimit) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that no reboot is requested
  // and the device does not reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that the grace period has been rescheduled to start at the time that
  // the update became available.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is running. The uptime limit is set to 24 hours. An update was applied
// and a reboot became necessary to complete the update process after 12 hours.
// The policy to automatically reboot after an update is enabled. The current
// uptime is 12 hours 20 seconds.
// Verifies that when the policy to reboot after an update is disabled, the
// grace period is rescheduled to start after 12 hours of uptime. Further
// verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, PolicyOffThenUptimeLimitOff) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has been rescheduled to end 24 hours from now.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Disable automatic reboot after an update has been applied. Verify that the
  // device does not reboot immediately.
  SetRebootAfterUpdate(false, false);

  // Verify that the grace period has been rescheduled to start after 24 hours
  // of uptime.
  VerifyGracePeriod(uptime_limit_);

  // Remove the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is set to 6 hours. An update was applied
// and a reboot became necessary to complete the update process after 12 hours.
// The policy to automatically reboot after an update is enabled. The current
// uptime is 12 hours 20 seconds.
// Verifies that when the uptime limit is removed, the grace period is
// rescheduled to have started after 12 hours of uptime. Further verifies that
// when the policy to reboot after an update is disabled, the reboot request and
// grace period are removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffThenPolicyOff) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that a reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that the grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Set the uptime limit. Verify that a reboot is requested again but the
  // device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that the grace period has been rescheduled to have started after
  // 6 hours of uptime.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::TimeDelta::FromSeconds(20), false);

  // Remove the uptime limit. Verify that a reboot is requested again but the
  // device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  SetUptimeLimit(base::TimeDelta(), false);

  // Verify that a grace period has been rescheduled to have started after 12
  // hours of uptime.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Disable automatic reboot after an update has been applied. Verify that the
  // device does not reboot immediately.
  SetRebootAfterUpdate(false, false);

  // Verify that the grace period has been removed.
  VerifyNoGracePeriod();

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The uptime limit is 6 hours. The current uptime is
// 29 hours 59 minutes 59 seconds.
// Verifies that if no non-kiosk-app session is in progress, the device reboots
// immediately when the grace period ends after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, GracePeriodEnd) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(29) +
                          base::TimeDelta::FromMinutes(59) +
                          base::TimeDelta::FromSeconds(59));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 1 second. Verify that unless a non-kiosk-app
  // session is in progress, the the device immediately reboots.
  FastForwardBy(base::TimeDelta::FromSeconds(1),
                !is_user_logged_in_ || is_kiosk_session());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. The current uptime is 10 days.
// Verifies that when no automatic reboot policy is enabled, no reboot occurs
// and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoPolicy) {
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. The uptime limit is set to 24 hours. The current uptime
// is 12 hours.
// Verifies that no reboot occurs and a grace period is scheduled to begin after
// 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartBeforeUptimeLimitGracePeriod) {
  SetUptimeLimit(base::TimeDelta::FromHours(24), false);
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. The uptime limit is set to 6 hours. The current uptime is
// 10 days.
// Verifies that if no non-kiosk-app session is in progress, the device reboots
// immediately because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartAfterUptimeLimitGracePeriod) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that a reboot is requested and unless a non-kiosk-app session is in
  // progress, the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  CreateAutomaticRebootManager(!is_user_logged_in_ || is_kiosk_session());
  VerifyRebootRequested(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. The uptime limit is set to 6 hours. The current uptime is
// 12 hours.
// Verifies that a reboot is requested and a grace period is started that will
// end after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartInUptimeLimitGracePeriod) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));

  // Verify that a reboot is requested but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  CreateAutomaticRebootManager(false);
  VerifyRebootRequested(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is
// enabled, the device reboots immediately if no non-kiosk-app session is in
// progress because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartAfterUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));
  SetRebootAfterUpdate(true, false);

  // Verify that a reboot is requested and unless a non-kiosk-app session is in
  // progress, the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  CreateAutomaticRebootManager(!is_user_logged_in_ || is_kiosk_session());
  VerifyRebootRequested(
      AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartInUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that a reboot is requested but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  CreateAutomaticRebootManager(false);
  VerifyRebootRequested(
      AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 10 minutes of uptime. The current uptime is
// 20 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, no reboot occurs and a grace period is scheduled to begin after the
// minimum of 1 hour of uptime.
TEST_P(AutomaticRebootManagerTest, StartBeforeUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromMinutes(10));
  uptime_provider()->SetUptime(base::TimeDelta::FromMinutes(20));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(base::TimeDelta::FromHours(1));

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is not
// enabled, no reboot occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartUpdateNoPolicy) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process but the time at which this happened was lost. The
// current uptime is 10 days.
// Verifies that the current uptime is persisted as the time at which a reboot
// became necessary. Further verifies that when the policy to automatically
// reboot after an update is enabled, a reboot is requested and a grace period
// is started that will end 24 hours from now.
TEST_P(AutomaticRebootManagerTest, StartUpdateTimeLost) {
  SetUpdateStatusNeedReboot();
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));
  SetRebootAfterUpdate(true, false);

  // Verify that a reboot is requested but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  CreateAutomaticRebootManager(false);
  VerifyRebootRequested(
      AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_ + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process but the time at which this happened was lost. The
// current uptime is 10 days.
// Verifies that the current uptime is persisted as the time at which a reboot
// became necessary. Further verifies that when the policy to automatically
// reboot after an update is not enabled, no reboot occurs and no grace period
// is scheduled.
TEST_P(AutomaticRebootManagerTest, StartUpdateNoPolicyTimeLost) {
  SetUpdateStatusNeedReboot();
  uptime_provider()->SetUptime(base::TimeDelta::FromDays(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. No update has been applied. The current uptime is
// 12 hours.
// Verifies that no time is persisted as the time at which a reboot became
// necessary. Further verifies that no reboot occurs and no grace period is
// scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoUpdate) {
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no time is persisted as the time at which a reboot became
  // necessary.
  EXPECT_FALSE(ReadUpdateRebootNeededUptimeFromFile(
      &update_reboot_needed_uptime_));

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is starting. The uptime limit is set to 6 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 8 hours of uptime. The current uptime is 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartUptimeLimitBeforeUpdate) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(8));
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that a reboot is requested but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  CreateAutomaticRebootManager(false);
  VerifyRebootRequested(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. The uptime limit is set to 8 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 6 hours of uptime. The current uptime is 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartUpdateBeforeUptimeLimit) {
  SetUptimeLimit(base::TimeDelta::FromHours(8), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  uptime_provider()->SetUptime(base::TimeDelta::FromHours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that a reboot is requested but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  CreateAutomaticRebootManager(false);
  VerifyRebootRequested(
      AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);

  // Verify that a grace period has started.
  VerifyGracePeriod(update_reboot_needed_uptime_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  FastForwardUntilNoTasksRemain(!is_user_logged_in_ || is_kiosk_session());
}

// Chrome is starting. The uptime limit is set to 6 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 6 hours of uptime. The current uptime is not available.
// Verifies that even if the policy to automatically reboot after an update is
// enabled, no reboot occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoUptime) {
  SetUptimeLimit(base::TimeDelta::FromHours(6), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::TimeDelta::FromHours(6));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

INSTANTIATE_TEST_SUITE_P(
    AutomaticRebootManagerTestInstance,
    AutomaticRebootManagerTest,
    ::testing::Values(
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN,
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION,
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_ARC_KIOSK_APP_SESSION,
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION));

}  // namespace system
}  // namespace chromeos
