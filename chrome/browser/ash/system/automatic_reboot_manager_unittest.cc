// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/automatic_reboot_manager.h"

#include <optional>
#include <string>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system/automatic_reboot_manager_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::ReturnPointee;

namespace ash {
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

  MockUptimeProvider(const MockUptimeProvider&) = delete;
  MockUptimeProvider& operator=(const MockUptimeProvider&) = delete;

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
  raw_ptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  base::FilePath uptime_file_path_;
  base::TimeDelta uptime_offset_;
};

class TestAutomaticRebootManagerTaskRunner
    : public base::TestMockTimeTaskRunner {
 public:
  TestAutomaticRebootManagerTaskRunner();

  TestAutomaticRebootManagerTaskRunner(
      const TestAutomaticRebootManagerTaskRunner&) = delete;
  TestAutomaticRebootManagerTaskRunner& operator=(
      const TestAutomaticRebootManagerTaskRunner&) = delete;

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
};

class MockAutomaticRebootManagerObserver
    : public AutomaticRebootManagerObserver {
 public:
  MockAutomaticRebootManagerObserver();

  MockAutomaticRebootManagerObserver(
      const MockAutomaticRebootManagerObserver&) = delete;
  MockAutomaticRebootManagerObserver& operator=(
      const MockAutomaticRebootManagerObserver&) = delete;

  ~MockAutomaticRebootManagerObserver() override;

  void Init(AutomaticRebootManager* automatic_reboot_manger);

  // AutomaticRebootManagerObserver:
  MOCK_METHOD1(OnRebootRequested, void(Reason));
  MOCK_METHOD0(WillDestroyAutomaticRebootManager, void());

 private:
  void StopObserving();

  raw_ptr<AutomaticRebootManager> automatic_reboot_manger_;
};

}  // namespace

class AutomaticRebootManagerBasicTest : public testing::Test {
 public:
  AutomaticRebootManagerBasicTest(const AutomaticRebootManagerBasicTest&) =
      delete;
  AutomaticRebootManagerBasicTest& operator=(
      const AutomaticRebootManagerBasicTest&) = delete;

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
  void NotifyTerminating(bool expect_reboot);

  void SleepFor(const base::TimeDelta& delta, bool expect_reboot);
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

  FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void LogIn(user_manager::User* user);

  MockUptimeProvider* uptime_provider() const {
    return task_runner_->uptime_provider();
  }

  template <class Timer>
  void VerifyTimerIsStopped(const Timer* timer) const;
  void VerifyTimerIsRunning(const base::OneShotTimer* timer,
                            const base::TimeDelta& delay) const;
  void VerifyTimerIsRunning(const base::WallClockTimer* timer,
                            const base::Time& desired_run_time) const;
  void VerifyLoginScreenIdleTimerIsRunning() const;

  // Shared account ID usable in each test.
  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("email", "123456");

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
  std::optional<base::ScopedPathOverride> file_uptime_override_;
  std::optional<base::ScopedPathOverride> file_reboot_needed_override_;
  base::FilePath update_reboot_needed_uptime_file_;

  bool reboot_after_update_ = false;

  base::test::TaskEnvironment task_environment_;
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      single_thread_task_runner_current_default_handle_override_;

  TestingPrefServiceSimple local_state_;
  user_manager::ScopedUserManager user_manager_enabler_;
  session_manager::SessionManager session_manager_;

  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> update_engine_client_ =
      nullptr;  // Not owned.
};

enum AutomaticRebootManagerTestScenario {
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN,
  AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION,
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
  ASSERT_TRUE(base::WriteFile(path, uptime_seconds));
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
      single_thread_task_runner_current_default_handle_override_(task_runner_),
      user_manager_enabler_(std::make_unique<FakeChromeUserManager>()) {}

AutomaticRebootManagerBasicTest::~AutomaticRebootManagerBasicTest() {
}

void AutomaticRebootManagerBasicTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath& temp_dir = temp_dir_.GetPath();
  const base::FilePath uptime_file = temp_dir.Append("uptime");
  uptime_provider()->set_uptime_file_path(uptime_file);
  ASSERT_TRUE(base::WriteFile(uptime_file, ""));
  update_reboot_needed_uptime_file_ =
      temp_dir.Append("update_reboot_needed_uptime");
  ASSERT_TRUE(base::WriteFile(update_reboot_needed_uptime_file_, ""));
  file_uptime_override_.emplace(FILE_UPTIME, uptime_file, /*is_absolute=*/false,
                                /*create=*/false);
  file_reboot_needed_override_.emplace(FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                                       update_reboot_needed_uptime_file_,
                                       /*is_absolute=*/false,
                                       /*create=*/false);

  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  AutomaticRebootManager::RegisterPrefs(local_state_.registry());

  update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();
  chromeos::PowerManagerClient::InitializeFake();
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

  chromeos::PowerManagerClient::Shutdown();
  UpdateEngineClient::Shutdown();
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
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
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
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
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyUpdateRebootNeeded() {
  SetUpdateStatusNeedReboot();
  automatic_reboot_manager_->UpdateStatusChanged(
      update_engine_client_->GetLastStatus());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::NotifyTerminating(bool expect_reboot) {
  automatic_reboot_manager_->OnAppTerminating();
  task_runner_->RunUntilIdle();
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::FastForwardBy(
    const base::TimeDelta& delta,
    bool expect_reboot) {
  task_runner_->FastForwardBy(delta);
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::SleepFor(const base::TimeDelta& delta,
                                               bool expect_reboot) {
  task_runner_->AdvanceWallClock(delta);
  automatic_reboot_manager_->SuspendDone(delta);
  if (automatic_reboot_manager_->grace_start_timer_ &&
      automatic_reboot_manager_->grace_start_timer_->IsRunning()) {
    automatic_reboot_manager_->grace_start_timer_->OnResume();
  }
  if (automatic_reboot_manager_->grace_end_timer_ &&
      automatic_reboot_manager_->grace_end_timer_->IsRunning()) {
    automatic_reboot_manager_->grace_end_timer_->OnResume();
  }
  task_runner_->RunUntilIdle();
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

void AutomaticRebootManagerBasicTest::FastForwardUntilNoTasksRemain(
    bool expect_reboot) {
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
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
      task_runner_->GetMockClock(), task_runner_->GetMockTickClock());
  automatic_reboot_manager_observer_.Init(automatic_reboot_manager_.get());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(
      expect_reboot ? 1 : 0,
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());

  uptime_processing_delay_ =
      base::TimeTicks() -
      automatic_reboot_manager_->boot_time_.value_or(base::TimeTicks()) -
      uptime_provider()->uptime();
  EXPECT_GE(uptime_processing_delay_, base::TimeDelta());
  EXPECT_LE(uptime_processing_delay_, base::Seconds(1));

  if (GetFakeUserManager()->IsUserLoggedIn() || expect_reboot) {
    VerifyLoginScreenIdleTimerIsStopped();
  } else {
    VerifyLoginScreenIdleTimerIsRunning();
  }
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
  *uptime = base::Milliseconds(seconds * 1000.0);
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
  const base::TimeDelta start_delay =
      start_uptime - uptime_provider()->uptime() - uptime_processing_delay_;
  const base::Time start = task_runner_->GetMockClock()->Now() + start_delay;
  const base::Time end = start + base::Hours(24);
  if (start_delay <= base::TimeDelta()) {
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

void AutomaticRebootManagerBasicTest::LogIn(user_manager::User* user) {
  const AccountId account_id = user->GetAccountId();
  std::string username_hash = user->username_hash();
  GetFakeUserManager()->UserLoggedIn(account_id, username_hash,
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
  session_manager_.CreateSession(account_id, username_hash, true);
  session_manager_.SessionStarted();
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
}

template <class Timer>
void AutomaticRebootManagerBasicTest::VerifyTimerIsStopped(
    const Timer* timer) const {
  if (timer)
    EXPECT_FALSE(timer->IsRunning());
}

void AutomaticRebootManagerBasicTest::VerifyTimerIsRunning(
    const base::OneShotTimer* timer,
    const base::TimeDelta& delay) const {
  ASSERT_TRUE(timer);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(delay, timer->GetCurrentDelay());
}

void AutomaticRebootManagerBasicTest::VerifyTimerIsRunning(
    const base::WallClockTimer* timer,
    const base::Time& desired_run_time) const {
  ASSERT_TRUE(timer);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(desired_run_time, timer->desired_run_time());
}

void AutomaticRebootManagerBasicTest::
    VerifyLoginScreenIdleTimerIsRunning() const {
  VerifyTimerIsRunning(
      automatic_reboot_manager_->login_screen_idle_timer_.get(),
      base::Seconds(60));
}

AutomaticRebootManagerTest::AutomaticRebootManagerTest() {
  auto* user_manager = GetFakeUserManager();

  switch (GetParam()) {
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_LOGIN_SCREEN:
      session_manager_.SetSessionState(
          session_manager::SessionState::LOGIN_PRIMARY);
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_KIOSK_APP_SESSION:
      LogIn(user_manager->AddKioskAppUser(account_id_));
      break;
    case AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION:
      LogIn(user_manager->AddUser(account_id_));
      break;
  }
}

AutomaticRebootManagerTest::~AutomaticRebootManagerTest() {
}

// Chrome is showing the login screen. The current uptime is 12 hours.
// Verifies that the idle timer is running. Further verifies that when a kiosk
// app session begins, the idle timer is stopped.
TEST_F(AutomaticRebootManagerBasicTest, LoginStopsIdleTimer) {
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested, the device does not reboot immediately
  // and the login screen idle timer is started.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that a kiosk app session has been started.
  LogIn(GetFakeUserManager()->AddKioskAppUser(account_id_));

  // Verify that the login screen idle timer is stopped.
  VerifyLoginScreenIdleTimerIsStopped();

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is showing the login screen. The current uptime is 12 hours.
// Verifies that the idle timer is running. Further verifies that when a
// non-kiosk-app session begins, the idle timer is stopped.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskLoginStopsIdleTimer) {
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested, the device does not reboot immediately
  // and the login screen idle timer is started.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that a non-kiosk-app session has been started.
  LogIn(GetFakeUserManager()->AddUser(account_id_));

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
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested, the device does not reboot immediately
  // and the login screen idle timer is started.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 25 minutes while simulating user activity every
  // 50 seconds.
  for (int i = 0; i < 30; ++i) {
    // Fast forward uptime by 50 seconds. Verify that the device does not reboot
    // immediately.
    FastForwardBy(base::Seconds(50), false);

    // Simulate user activity.
    automatic_reboot_manager_->OnUserActivity(nullptr);
  }

  // Fast forward the uptime by 60 seconds without simulating user activity.
  // Verify that the device reboots immediately.
  FastForwardBy(base::Seconds(60), true);
}

// Chrome is running a kiosk app session. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, ResumeNoPolicy) {
  LogIn(GetFakeUserManager()->AddKioskAppUser(account_id_));
  uptime_provider()->SetUptime(base::Days(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a non-kiosk-app session. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeAppNoPolicy) {
  LogIn(GetFakeUserManager()->AddUser(account_id_));
  uptime_provider()->SetUptime(base::Days(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is on the login screen. The current uptime is 10 days.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, LoginScreenResumeNoPolicy) {
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  uptime_provider()->SetUptime(base::Days(10));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that a reboot is never requested and the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 24 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, ResumeBeforeGracePeriod) {
  LogIn(GetFakeUserManager()->AddKioskAppUser(account_id_));
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify a reboot is requested and the device reboots eventually.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 24 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeBeforeGracePeriod) {
  LogIn(GetFakeUserManager()->AddUser(account_id_));
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that a reboot is requested eventually but the device never reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is on the login screen. The uptime limit is 24 hours. The current
// uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, LoginScreenResumeBeforeGracePeriod) {
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that no
  // reboot is requested and the device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that a reboot is requested eventually but the device never reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  FastForwardUntilNoTasksRemain(true);
}

// Chrome is running a kiosk app session. The uptime limit is 6 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it reboots
// shortly after.
TEST_F(AutomaticRebootManagerBasicTest, ResumeInGracePeriod) {
  LogIn(GetFakeUserManager()->AddKioskAppUser(account_id_));
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device reboots after additional 1 second.
  SleepFor(base::Hours(1), false);
  FastForwardBy(base::Seconds(1), true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 6 hours. The
// current uptime is 12 hours.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeInGracePeriod) {
  LogIn(GetFakeUserManager()->AddUser(account_id_));
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running a kiosk app session. The uptime limit is 6 hours. The
// current uptime is 29 hours 30 minutes.
// Verifies that when the device is suspended and then resumes, it immediately
// reboots.
TEST_F(AutomaticRebootManagerBasicTest, ResumeAfterGracePeriod) {
  LogIn(GetFakeUserManager()->AddKioskAppUser(account_id_));
  uptime_provider()->SetUptime(base::Hours(29) + base::Minutes(30));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device reboots immediately.
  SleepFor(base::Hours(1), true);
}

// Chrome is running a non-kiosk-app session. The uptime limit is 6 hours. The
// current uptime is 29 hours 30 minutes.
// Verifies that when the device is suspended and then resumes, it does not
// immediately reboot.
TEST_F(AutomaticRebootManagerBasicTest, NonKioskResumeAfterGracePeriod) {
  LogIn(GetFakeUserManager()->AddUser(account_id_));
  uptime_provider()->SetUptime(base::Hours(29) + base::Minutes(30));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the device has resumed from 1 hour of sleep. Verify that the
  // device does not reboot immediately.
  SleepFor(base::Hours(1), false);

  // Verify that the device never reboots.
  FastForwardUntilNoTasksRemain(false);
}

// Chrome is running. The current uptime is 10 days.
// Verifies that when the browser terminates, the device does not immediately
// reboot.
TEST_P(AutomaticRebootManagerTest, TerminateNoPolicy) {
  uptime_provider()->SetUptime(base::Days(10));

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
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the browser is terminating. Verify that no reboot is requested
  // and the device does not reboot immediately.
  NotifyTerminating(false);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 6 hours. The current uptime is
// 12 hours.
// Verifies that when the browser terminates, the device immediately reboots if
// a kiosk app session is in progress.
TEST_P(AutomaticRebootManagerTest, TerminateInGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Notify that the browser is terminating. Verify that the device immediately
  // reboots if a kiosk app session is in progress.
  auto* user_manager = GetFakeUserManager();
  NotifyTerminating(user_manager->IsLoggedInAsAnyKioskApp());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when the uptime limit is set to 24 hours, no reboot occurs and
// a grace period is scheduled to begin after 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, BeforeUptimeLimitGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when the uptime limit is set to 6 hours, a reboot is requested
// and a grace period is started that will end after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, InUptimeLimitGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(12));

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
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 10 days.
// Verifies that when the uptime limit is set to 6 hours, the device reboots
// immediately if no non-kiosk-app-session is in progress because the grace
// period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, AfterUptimeLimitGracePeriod) {
  uptime_provider()->SetUptime(base::Days(10));

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
  auto* user_manager = GetFakeUserManager();
  SetUptimeLimit(base::Hours(6), !user_manager->IsUserLoggedIn() ||
                                     user_manager->IsLoggedInAsAnyKioskApp());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 6 hours.
// Verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, UptimeLimitOffBeforeGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(6));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(12), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 1 hour. Verify that no reboot is requested and
  // the device does not reboot immediately.
  FastForwardBy(base::Hours(1), false);

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
  uptime_provider()->SetUptime(base::Hours(24));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(12), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::Seconds(20), false);

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
  uptime_provider()->SetUptime(base::Hours(6));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(12), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that no reboot is requested
  // and the device does not reboot immediately.
  FastForwardBy(base::Seconds(20), false);

  // Extend the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that the grace period has been rescheduled to start further in the
  // future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 12 hours. The current uptime is
// 18 hours.
// Verifies that when the uptime limit is extended to 24 hours, the grace period
// is rescheduled to start in the future.
TEST_P(AutomaticRebootManagerTest, ExtendUptimeLimitInGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(18));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(12), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::Seconds(20), false);

  // Extend the uptime limit. Verify that the device does not reboot
  // immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that the grace period has been rescheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Verify that a reboot is requested again eventually and unless a
  // non-kiosk-app session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 18 hours. The current uptime is
// 12 hours.
// Verifies that when the uptime limit is shortened to 6 hours, the grace period
// is rescheduled to have already started.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitBeforeToInGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(18), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that no reboot is requested
  // and the device does not reboot immediately.
  FastForwardBy(base::Seconds(20), false);

  // Shorten the uptime limit. Verify that a reboot is requested but the device
  // does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that the grace period has been rescheduled and has started already.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 36 hours.
// Verifies that when the uptime limit is shortened to 18 hours, the grace
// period is rescheduled to have started earlier.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitInToInGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(36));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::Seconds(20), false);

  // Shorten the uptime limit. Verify that a reboot is requested again but the
  // device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(18), false);

  // Verify that the grace period has been rescheduled to have started earlier.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 24 hours. The current uptime is
// 36 hours.
// Verifies that when the uptime limit is shortened to 6 hours, the device
// reboots immediately if no non-kiosk-app session is in progress because the
// grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, ShortenUptimeLimitInToAfterGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(36));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::Seconds(20), false);

  // Shorten the uptime limit. Verify that a reboot is requested again and
  // unless a non-kiosk-app session is in progress, the the device immediately
  // reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  auto* user_manager = GetFakeUserManager();
  SetUptimeLimit(base::Hours(6), !user_manager->IsUserLoggedIn() ||
                                     user_manager->IsLoggedInAsAnyKioskApp());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when an update is applied, the current uptime is persisted as
// the time at which a reboot became necessary. Further verifies that when the
// policy to automatically reboot after an update is not enabled, no reboot
// occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, UpdateNoPolicy) {
  uptime_provider()->SetUptime(base::Hours(12));

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
  uptime_provider()->SetUptime(base::Hours(12));
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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 12 hours.
// Verifies that when Chrome is notified twice that an update has been applied,
// the second notification is ignored and the uptime at which it occurred does
// not get persisted as the time at which an update became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateAfterUpdate) {
  uptime_provider()->SetUptime(base::Hours(12));
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
  FastForwardBy(base::Seconds(20), false);

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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 10 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, no reboot occurs and a grace period is scheduled to begin after the
// minimum of 1 hour of uptime. Further verifies that when an update is applied,
// the current uptime is persisted as the time at which a reboot became
// necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeMinimumUptime) {
  uptime_provider()->SetUptime(base::Minutes(10));
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
  VerifyGracePeriod(base::Hours(1));

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 10 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled and the kMinRebootUptimeMs switch is set to 20 minutes, no reboot
// occurs and a grace period is scheduled to begin after 20 minutes of uptime.
// Further verifies that when an update is applied, the current uptime is
// persisted as the time at which a reboot became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeMinimumUptimeWithSwitch) {
  uptime_provider()->SetUptime(base::Minutes(10));
  // Set --min-reboot-uptime-ms flag to 20*60*1000 ms == 20 mins.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--min-reboot-uptime-ms=1200000"});
  SetRebootAfterUpdate(/*reboot_after_update*/ true, /*expect_reboot*/ false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(/*expect_reboot*/ false);
  VerifyNoRebootRequested();

  // Verify that no grace period has started.
  VerifyNoGracePeriod();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Verify that the current uptime has been persisted as the time at which a
  // reboot became necessary.
  EXPECT_TRUE(
      ReadUpdateRebootNeededUptimeFromFile(&update_reboot_needed_uptime_));
  EXPECT_EQ(uptime_provider()->uptime(), update_reboot_needed_uptime_);

  // Verify that a grace period has been scheduled to begin in the future.
  VerifyGracePeriod(base::Minutes(20));

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The current uptime is 10 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, device is suspended for 1 hour and resumes in grace period, it
// immediately reboots if no session is active or shortly after if non-kiosk-app
// session is in progress.
TEST_P(AutomaticRebootManagerTest, UpdateAndSuspendUntilInGracePeriod) {
  uptime_provider()->SetUptime(base::Minutes(10));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Stay idle for 2 minutes on login screen. Verify no reboot is requested.
  auto* user_manager = GetFakeUserManager();
  if (!user_manager->IsUserLoggedIn()) {
    FastForwardBy(base::Minutes(2), false);
  }

  // Simulate sleep for 1 hour. Verify that device immediately reboots unless a
  // session is in progress.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  SleepFor(base::Hours(1), !user_manager->IsUserLoggedIn());

  // Wait for 1 more second in case a session is active. Verify that device
  // immediately reboots if it is a kiosk session.
  if (user_manager->IsUserLoggedIn()) {
    FastForwardBy(base::Seconds(1), user_manager->IsLoggedInAsAnyKioskApp());
  }
}

// Chrome is running. The current uptime is 10 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, device is suspended for 25 hours and resumes after grace period, it
// immediately reboots unless a non-kiosk-app session is in progress.
TEST_P(AutomaticRebootManagerTest, UpdateAndSuspendUntilAfterGracePeriod) {
  uptime_provider()->SetUptime(base::Minutes(10));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Notify that an update has been applied and a reboot is necessary. Verify
  // that no reboot is requested and the device does not reboot immediately.
  NotifyUpdateRebootNeeded();

  // Simulate sleep for 25 hours. Verify that device immediately reboots unless
  // a non-kiosk-app session is in progress.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  auto* user_manager = GetFakeUserManager();
  SleepFor(base::Hours(25), !user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, PolicyAfterUpdateInGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(6));

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
  FastForwardBy(base::Hours(6), false);

  // Simulate user activity.
  automatic_reboot_manager_->OnUserActivity(nullptr);

  // Enable automatic reboot after an update has been applied. Verify that a
  // reboot is requested but the device does not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  SetRebootAfterUpdate(true, false);

  // Verify that a grace period has started.
  VerifyGracePeriod(base::Hours(6) + uptime_processing_delay_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is
// enabled, the device reboots immediately if no non-kiosk-app session is in
// progress because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, PolicyAfterUpdateAfterGracePeriod) {
  uptime_provider()->SetUptime(base::Hours(6));

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
  FastForwardBy(base::Days(10) - base::Hours(6), false);

  // Simulate user activity.
  automatic_reboot_manager_->OnUserActivity(nullptr);

  // Enable automatic rebooting after an update has been applied. Verify that
  // a reboot is requested and unless a non-kiosk-app session is in progress,
  // the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  auto* user_manager = GetFakeUserManager();
  SetRebootAfterUpdate(true, !user_manager->IsUserLoggedIn() ||
                                 user_manager->IsLoggedInAsAnyKioskApp());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The policy to
// automatically reboot after an update is enabled. The current uptime is
// 6 hours 20 seconds.
// Verifies that when the policy to automatically reboot after an update is
// disabled, the reboot request and grace period are removed.
TEST_P(AutomaticRebootManagerTest, PolicyOffAfterUpdate) {
  uptime_provider()->SetUptime(base::Hours(6));
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
  FastForwardBy(base::Seconds(20), false);

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
  SetUptimeLimit(base::Hours(6), false);

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
  uptime_provider()->SetUptime(base::Hours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::Seconds(20), false);

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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The policy to automatically reboot after an update is
// enabled. The current uptime is 12 hours.
// Verifies that when an uptime limit of 24 hours is set, the availability of an
// update causes the grace period to be rescheduled so that it ends 24 hours
// from now. Further verifies that the current uptime is persisted as the time
// at which a reboot became necessary.
TEST_P(AutomaticRebootManagerTest, UpdateBeforeUptimeLimit) {
  uptime_provider()->SetUptime(base::Hours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that no reboot is requested
  // and the device does not reboot immediately.
  FastForwardBy(base::Seconds(20), false);

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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is running. The uptime limit is set to 24 hours. An update was applied
// and a reboot became necessary to complete the update process after 12 hours.
// The policy to automatically reboot after an update is enabled. The current
// uptime is 12 hours 20 seconds.
// Verifies that when the policy to reboot after an update is disabled, the
// grace period is rescheduled to start after 12 hours of uptime. Further
// verifies that when the uptime limit is removed, the grace period is removed.
TEST_P(AutomaticRebootManagerTest, PolicyOffThenUptimeLimitOff) {
  uptime_provider()->SetUptime(base::Hours(12));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that no reboot is requested and the device
  // does not reboot immediately.
  SetUptimeLimit(base::Hours(24), false);

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
  FastForwardBy(base::Seconds(20), false);

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
  uptime_provider()->SetUptime(base::Hours(12));
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
  SetUptimeLimit(base::Hours(6), false);

  // Verify that the grace period has been rescheduled to have started after
  // 6 hours of uptime.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 20 seconds. Verify that the device does not
  // reboot immediately.
  FastForwardBy(base::Seconds(20), false);

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
  uptime_provider()->SetUptime(base::Hours(29) + base::Minutes(59) +
                               base::Seconds(59));

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Set the uptime limit. Verify that a reboot is requested but the device does
  // not reboot immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  SetUptimeLimit(base::Hours(6), false);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Fast forward the uptime by 1 second. Verify that unless a non-kiosk-app
  // session is in progress, the the device immediately reboots.
  auto* user_manager = GetFakeUserManager();
  FastForwardBy(base::Seconds(1), !user_manager->IsUserLoggedIn() ||
                                      user_manager->IsLoggedInAsAnyKioskApp());

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. The current uptime is 10 days.
// Verifies that when no automatic reboot policy is enabled, no reboot occurs
// and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoPolicy) {
  uptime_provider()->SetUptime(base::Days(10));

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
  SetUptimeLimit(base::Hours(24), false);
  uptime_provider()->SetUptime(base::Hours(12));

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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. The uptime limit is set to 6 hours. The current uptime is
// 10 days.
// Verifies that if no non-kiosk-app session is in progress, the device reboots
// immediately because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartAfterUptimeLimitGracePeriod) {
  SetUptimeLimit(base::Hours(6), false);
  uptime_provider()->SetUptime(base::Days(10));

  // Verify that a reboot is requested and unless a non-kiosk-app session is in
  // progress, the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  auto* user_manager = GetFakeUserManager();
  CreateAutomaticRebootManager(!user_manager->IsUserLoggedIn() ||
                               user_manager->IsLoggedInAsAnyKioskApp());
  VerifyRebootRequested(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. The uptime limit is set to 6 hours. The current uptime is
// 12 hours.
// Verifies that a reboot is requested and a grace period is started that will
// end after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartInUptimeLimitGracePeriod) {
  SetUptimeLimit(base::Hours(6), false);
  uptime_provider()->SetUptime(base::Hours(12));

  // Verify that a reboot is requested but the device does not reboot
  // immediately.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);
  CreateAutomaticRebootManager(false);
  VerifyRebootRequested(AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC);

  // Verify that a grace period has started.
  VerifyGracePeriod(uptime_limit_);

  // Verify that unless a non-kiosk-app session is in progress, the device
  // eventually reboots.
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is
// enabled, the device reboots immediately if no non-kiosk-app session is in
// progress because the grace period ended after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartAfterUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Hours(6));
  uptime_provider()->SetUptime(base::Days(10));
  SetRebootAfterUpdate(true, false);

  // Verify that a reboot is requested and unless a non-kiosk-app session is in
  // progress, the the device immediately reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  auto* user_manager = GetFakeUserManager();
  CreateAutomaticRebootManager(!user_manager->IsUserLoggedIn() ||
                               user_manager->IsLoggedInAsAnyKioskApp());
  VerifyRebootRequested(
      AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);

  // Verify that if a non-kiosk-app session is in progress, the device never
  // reboots.
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartInUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Hours(6));
  uptime_provider()->SetUptime(base::Hours(12));
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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 10 minutes of uptime. The current uptime is
// 20 minutes.
// Verifies that when the policy to automatically reboot after an update is
// enabled, no reboot occurs and a grace period is scheduled to begin after the
// minimum of 1 hour of uptime.
TEST_P(AutomaticRebootManagerTest, StartBeforeUpdateGracePeriod) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Minutes(10));
  uptime_provider()->SetUptime(base::Minutes(20));
  SetRebootAfterUpdate(true, false);

  // Verify that no reboot is requested and the device does not reboot
  // immediately.
  ExpectNoRebootRequest();
  CreateAutomaticRebootManager(false);
  VerifyNoRebootRequested();

  // Verify that a grace period has been scheduled to start in the future.
  VerifyGracePeriod(base::Hours(1));

  // Verify that a reboot is requested eventually and unless a non-kiosk-app
  // session is in progress, the device eventually reboots.
  ExpectRebootRequest(AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE);
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. An update was applied and a reboot became necessary to
// complete the update process after 6 hours of uptime. The current uptime is
// 10 days.
// Verifies that when the policy to automatically reboot after an update is not
// enabled, no reboot occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartUpdateNoPolicy) {
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Hours(6));
  uptime_provider()->SetUptime(base::Days(10));

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
  uptime_provider()->SetUptime(base::Days(10));
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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
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
  uptime_provider()->SetUptime(base::Days(10));

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
  uptime_provider()->SetUptime(base::Hours(12));
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
  SetUptimeLimit(base::Hours(6), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Hours(8));
  uptime_provider()->SetUptime(base::Hours(12));
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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. The uptime limit is set to 8 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 6 hours of uptime. The current uptime is 12 hours.
// Verifies that when the policy to automatically reboot after an update is
// enabled, a reboot is requested and a grace period is started that will end
// after 6 + 24 hours of uptime.
TEST_P(AutomaticRebootManagerTest, StartUpdateBeforeUptimeLimit) {
  SetUptimeLimit(base::Hours(8), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Hours(6));
  uptime_provider()->SetUptime(base::Hours(12));
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
  auto* user_manager = GetFakeUserManager();
  FastForwardUntilNoTasksRemain(!user_manager->IsUserLoggedIn() ||
                                user_manager->IsLoggedInAsAnyKioskApp());
}

// Chrome is starting. The uptime limit is set to 6 hours. Also, an update was
// applied and a reboot became necessary to complete the update process after
// 6 hours of uptime. The current uptime is not available.
// Verifies that even if the policy to automatically reboot after an update is
// enabled, no reboot occurs and no grace period is scheduled.
TEST_P(AutomaticRebootManagerTest, StartNoUptime) {
  SetUptimeLimit(base::Hours(6), false);
  SetUpdateStatusNeedReboot();
  SetUpdateRebootNeededUptime(base::Hours(6));
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
        AUTOMATIC_REBOOT_MANAGER_TEST_SCENARIO_NON_KIOSK_APP_SESSION));

}  // namespace system
}  // namespace ash
