// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/arc_data_snapshotd_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/enterprise/arc_data_snapshotd_bridge.h"
#include "ash/components/arc/enterprise/snapshot_session_controller.h"
#include "ash/components/arc/test/fake_apps_tracker.h"
#include "ash/components/arc/test/fake_snapshot_reboot_notification.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_data_snapshotd_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_switches.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::WithArgs;

class PrefService;

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kPublicAccountEmail[] = "public-account@localhost";

class TestUpstartClient : public ash::FakeUpstartClient {
 public:
  // FakeUpstartClient overrides:
  MOCK_METHOD(void,
              StartArcDataSnapshotd,
              (const std::vector<std::string>&,
               chromeos::VoidDBusMethodCallback),
              (override));

  MOCK_METHOD(void,
              StopArcDataSnapshotd,
              (chromeos::VoidDBusMethodCallback),
              (override));
};

class FakeDelegate : public ArcDataSnapshotdManager::Delegate {
 public:
  FakeDelegate() { arc::prefs::RegisterProfilePrefs(pref_service_.registry()); }
  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;
  ~FakeDelegate() override = default;

  // ArcDataSnapshotdManager::Delegate overrides:
  void RequestStopArcInstance(
      base::OnceCallback<void(bool)> stopped_callback) override {
    EXPECT_FALSE(stopped_callback.is_null());
    stopped_callback_num_++;
    std::move(stopped_callback).Run(true /* success */);
  }

  PrefService* GetProfilePrefService() override { return &pref_service_; }

  std::unique_ptr<ArcSnapshotRebootNotification> CreateRebootNotification()
      override {
    return std::make_unique<FakeSnapshotRebootNotification>();
  }

  std::unique_ptr<ArcAppsTracker> CreateAppsTracker() override {
    return std::make_unique<FakeAppsTracker>();
  }

  void RestartChrome(const base::CommandLine& cmd) override {
    EXPECT_EQ(cmd.GetSwitchValueASCII(switches::kOzonePlatform), kHeadless);
  }

  bool stopped_callback_num() const { return stopped_callback_num_; }

 private:
  TestingPrefServiceSimple pref_service_;

  int stopped_callback_num_ = 0;
};

class FakeSnapshotSessionController : public SnapshotSessionController {
 public:
  explicit FakeSnapshotSessionController(
      std::unique_ptr<ArcAppsTracker> apps_tracker)
      : apps_tracker_(std::move(apps_tracker)) {}
  ~FakeSnapshotSessionController() override = default;

  void AddObserver(Observer* observer) override {
    EXPECT_FALSE(observer_);
    observer_ = observer;
  }
  void RemoveObserver(Observer* observer) override {
    EXPECT_EQ(observer_, observer);
    observer_ = nullptr;
  }

  void StartSession() {
    apps_tracker_->StartTracking(
        base::BindRepeating(
            &FakeSnapshotSessionController::NotifySnapshotAppInstalled,
            base::Unretained(this)),
        base::BindOnce(
            &FakeSnapshotSessionController::NotifySnapshotPolicyCompliant,
            base::Unretained(this)));
    NotifySnapshotSessionStarted();
  }

  void StopSession(bool success) {
    if (success)
      NotifySnapshotSessionStopped();
    else
      NotifySnapshotSessionFailed();
  }

 private:
  void NotifySnapshotSessionStarted() {
    EXPECT_TRUE(observer_);
    observer_->OnSnapshotSessionStarted();
  }
  void NotifySnapshotSessionStopped() {
    EXPECT_TRUE(observer_);
    observer_->OnSnapshotSessionStopped();
  }
  void NotifySnapshotSessionFailed() {
    EXPECT_TRUE(observer_);
    observer_->OnSnapshotSessionFailed();
  }
  void NotifySnapshotAppInstalled(int percent) {
    EXPECT_TRUE(observer_);
    observer_->OnSnapshotAppInstalled(percent);
  }
  void NotifySnapshotPolicyCompliant() {
    EXPECT_TRUE(observer_);
    observer_->OnSnapshotSessionPolicyCompliant();
  }

  std::unique_ptr<ArcAppsTracker> apps_tracker_;
  // Owned by manager.
  Observer* observer_ = nullptr;
};

// Basic tests for ArcDataSnapshotdManager class instance.
class ArcDataSnapshotdManagerBasicTest : public testing::Test {
 protected:
  ArcDataSnapshotdManagerBasicTest() {
    ash::ArcDataSnapshotdClient::InitializeFake();

    fake_user_manager_ = new user_manager::FakeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    upstart_client_ = std::make_unique<TestUpstartClient>();

    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());
    local_state_.SetInitializationCompleted();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kFirstExecAfterBoot);
  }

  void SetUp() override { SetDBusClientAvailability(true /* is_available */); }

  void TearDown() override {
    ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(
        false /* enabled */);
    manager_.reset();
    apps_tracker_ = nullptr;
    delegate_ = nullptr;
    ClearLocalState();
  }

  ~ArcDataSnapshotdManagerBasicTest() override {
    ash::ArcDataSnapshotdClient::Shutdown();
  }

  void ExpectStartDaemon(bool success,
                         const std::vector<std::string>& env = {}) {
    EXPECT_CALL(*upstart_client(), StartArcDataSnapshotd(Eq(env), _))
        .WillOnce(WithArgs<1>(
            Invoke([success](chromeos::VoidDBusMethodCallback callback) {
              std::move(callback).Run(success);
            })));
  }

  void ExpectStopDaemon(bool success) {
    EXPECT_CALL(*upstart_client(), StopArcDataSnapshotd(_))
        .WillOnce(WithArgs<0>(
            Invoke([success](chromeos::VoidDBusMethodCallback callback) {
              std::move(callback).Run(success);
            })));
  }

  void SetUpRestoredSessionCommandLine() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(ash::switches::kLoginUser);
  }

  void SetDBusClientAvailability(bool is_available) {
    auto* client = static_cast<ash::FakeArcDataSnapshotdClient*>(
        ash::ArcDataSnapshotdClient::Get());
    DCHECK(client);
    client->set_available(is_available);
  }

  ArcDataSnapshotdManager* CreateManager(
      base::OnceClosure attempt_exit_callback = base::DoNothing()) {
    manager_ = std::make_unique<ArcDataSnapshotdManager>(
        local_state(), MakeDelegate(), std::move(attempt_exit_callback));
    manager_->set_session_controller_for_testing(MakeSessionController());
    session_controller_->AddObserver(manager_.get());
    return manager_.get();
  }

  // Check number of snapshots in local_state.
  void CheckSnapshots(int expected_snapshots_number,
                      bool expected_blocked_ui = true,
                      bool expected_snapshot_started = false) {
    ArcDataSnapshotdManager::Snapshot snapshot(local_state());
    snapshot.Parse();
    int actual_number = 0;
    if (snapshot.previous_snapshot()) {
      EXPECT_FALSE(snapshot.previous_snapshot()->is_last());
      actual_number++;
    }
    if (snapshot.last_snapshot()) {
      EXPECT_TRUE(snapshot.last_snapshot()->is_last());
      actual_number++;
    }
    EXPECT_EQ(expected_snapshots_number, actual_number);
    EXPECT_EQ(expected_blocked_ui, snapshot.is_blocked_ui_mode());
    EXPECT_EQ(expected_snapshot_started, snapshot.started());
  }

  void CheckVerifiedLastSnapshot() {
    ArcDataSnapshotdManager::Snapshot snapshot(local_state());
    snapshot.Parse();

    EXPECT_TRUE(snapshot.last_snapshot());
    EXPECT_TRUE(snapshot.last_snapshot()->is_verified());
  }

  void ExpectStartTrackingApps() {
    EXPECT_EQ(1, apps_tracker()->start_tracking_num());
    EXPECT_FALSE(apps_tracker()->update_callback().is_null());
  }

  void LoginAsPublicSession() {
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    user_manager()->AddPublicAccountUser(account_id);
    user_manager()->UserLoggedIn(account_id, account_id.GetUserEmail(), false,
                                 false);
  }

  void LogoutPublicSession() {
    user_manager()->LogoutAllUsers();
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    user_manager()->RemoveUserFromList(account_id);
  }

  // Set up local_state with info for previous and last snapshots and blocked ui
  // mode.
  void SetupLocalState(bool blocked_ui_mode) {
    auto last_snapshot =
        ArcDataSnapshotdManager::SnapshotInfo::CreateForTesting(
            base::SysInfo::OperatingSystemVersion(), base::Time::Now(),
            false /* verified */, false /* updated */, true /* is_last */);
    auto previous_snapshot =
        ArcDataSnapshotdManager::SnapshotInfo::CreateForTesting(
            base::SysInfo::OperatingSystemVersion(), base::Time::Now(),
            false /* verified */, false /* updated */, false /* is_last */);
    auto snapshot = ArcDataSnapshotdManager::Snapshot::CreateForTesting(
        local_state(), blocked_ui_mode, false /* started */,
        std::move(last_snapshot), std::move(previous_snapshot));
    snapshot->Sync();
  }

  void RequestArcDataRemoval() {
    delegate_->GetProfilePrefService()->SetBoolean(
        prefs::kArcDataRemoveRequested, true);
  }

  virtual void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestUpstartClient* upstart_client() { return upstart_client_.get(); }
  PrefService* local_state() { return &local_state_; }
  user_manager::FakeUserManager* user_manager() { return fake_user_manager_; }
  FakeAppsTracker* apps_tracker() { return apps_tracker_; }
  FakeDelegate* delegate() { return delegate_; }
  FakeSnapshotSessionController* session_controller() {
    return session_controller_;
  }

  ash::FakeArcDataSnapshotdClient* client() const {
    return static_cast<ash::FakeArcDataSnapshotdClient*>(
        ash::ArcDataSnapshotdClient::Get());
  }

 protected:
  std::unique_ptr<ArcDataSnapshotdManager::Delegate> MakeDelegate() {
    auto delegate = std::make_unique<FakeDelegate>();
    delegate_ = delegate.get();
    return delegate;
  }

  std::unique_ptr<ArcAppsTracker> MakeAppsTracker() {
    auto apps_tracker = std::make_unique<FakeAppsTracker>();
    apps_tracker_ = apps_tracker.get();
    return apps_tracker;
  }

  std::unique_ptr<FakeSnapshotSessionController> MakeSessionController() {
    auto session_controller =
        std::make_unique<FakeSnapshotSessionController>(MakeAppsTracker());
    session_controller_ = session_controller.get();
    return session_controller;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;

 private:
  void ClearLocalState() {
    auto snapshot = ArcDataSnapshotdManager::Snapshot::CreateForTesting(
        local_state(), false /* blocked_ui_mode */, false /* started */,
        nullptr /* last_snapshot */, nullptr /* previous_snapshot */);
    snapshot->Sync();
  }

  std::unique_ptr<TestUpstartClient> upstart_client_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<ArcDataSnapshotdManager> manager_;

  // Owned by |manager_|.
  FakeDelegate* delegate_ = nullptr;
  FakeAppsTracker* apps_tracker_ = nullptr;
  FakeSnapshotSessionController* session_controller_ = nullptr;

  user_manager::FakeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

class ArcDataSnapshotdManagerStateTest
    : public ArcDataSnapshotdManagerBasicTest,
      public ::testing::WithParamInterface<ArcDataSnapshotdManager::State> {
 public:
  ArcDataSnapshotdManager::State expected_state() { return GetParam(); }

  // Expire snapshots in max lifetime.
  void ExpireSnapshots() {
    task_environment_.FastForwardBy(
        ArcDataSnapshotdManager::snapshot_max_lifetime_for_testing());
    task_environment_.RunUntilIdle();
  }
};

// Tests flows in ArcDataSnapshotdManager:
// * clear snapshot flow.
// * generate key pair flow.
// * blocked UI flow.
class ArcDataSnapshotdManagerFlowTest
    : public ArcDataSnapshotdManagerBasicTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    SetDBusClientAvailability(is_dbus_client_available());
  }

  bool is_dbus_client_available() { return GetParam(); }

  void EnableHeadlessMode() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kOzonePlatform, kHeadless);
  }

  void RunUntilIdle() override {
    if (is_dbus_client_available()) {
      task_environment_.RunUntilIdle();
      return;
    }
    size_t attempts_number =
        ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing();
    for (size_t i = 0; i < attempts_number; i++) {
      task_environment_.FastForwardBy(
          ArcDataSnapshotdBridge::connection_attempt_interval_for_testing());
      task_environment_.RunUntilIdle();
    }
  }
};

// Test basic scenario: start / stop arc-data-snapshotd.
TEST_F(ArcDataSnapshotdManagerBasicTest, Basic) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager->bridge());

  ExpectStartDaemon(true /* success */);
  manager->EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager->bridge());

  ExpectStopDaemon(true /* success */);
  manager->EnsureDaemonStopped(base::DoNothing());
  EXPECT_FALSE(manager->bridge());
}

// Test a double start scenario: start arc-data-snapshotd twice.
// Upstart job returns "false" if the job is already running.
TEST_F(ArcDataSnapshotdManagerBasicTest, DoubleStart) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager->bridge());

  ExpectStartDaemon(true /* success */);
  manager->EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager->bridge());

  // The attempt to start the already running daemon.
  // upstart client is not aware of this.
  manager->EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager->bridge());

  // Stop daemon from dtor.
  ExpectStopDaemon(true /* success */);
}

// Test that arc-data-snapshotd daemon is already running when |manager| gets
// created.
// Test that arc-data-snapshotd daemon is already stopped when |manager| tries
// to stop it.
TEST_F(ArcDataSnapshotdManagerBasicTest, UpstartFailures) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);

  auto* manager = CreateManager();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager->bridge());

  ExpectStartDaemon(false /* success */);
  manager->EnsureDaemonStarted(base::DoNothing());
  EXPECT_TRUE(manager->bridge());

  ExpectStopDaemon(false /* success */);
  manager->EnsureDaemonStopped(base::DoNothing());
  EXPECT_FALSE(manager->bridge());
}

TEST_F(ArcDataSnapshotdManagerBasicTest, RestoredAfterCrash) {
  SetUpRestoredSessionCommandLine();
  // The attempt to stop the daemon, started before crash.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kRestored);
  EXPECT_FALSE(manager->IsAutoLoginConfigured());
  EXPECT_TRUE(manager->IsAutoLoginAllowed());

  EXPECT_FALSE(manager->bridge());

  ExpectStartDaemon(true /* success */);
  manager->EnsureDaemonStarted(base::DoNothing());

  // Stop daemon from dtor.
  ExpectStopDaemon(true /* success */);
}

// Test failure LoadSnapshot flow when no user is logged in.
TEST_F(ArcDataSnapshotdManagerBasicTest, LoadSnapshotsFailureNoUser) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(false /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Stop daemon, nothing to do.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager();
  // No snapshots in local_state either.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  EXPECT_FALSE(manager->bridge());

  base::RunLoop run_loop;
  manager->StartLoadingSnapshot(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test failure LoadSnapshot flow when no available snapshots.
TEST_F(ArcDataSnapshotdManagerBasicTest, LoadSnapshotsFailureNoSnapshots) {
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Stop daemon, nothing to do.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager();
  // No snapshots in local_state either.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  EXPECT_FALSE(manager->bridge());

  LoginAsPublicSession();
  base::RunLoop run_loop;
  manager->StartLoadingSnapshot(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
}

// Test failure LoadSnapshot flow when the snapshot functionality is disabled
TEST_F(ArcDataSnapshotdManagerBasicTest, LoadSnapshotsFailureDisabled) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(false /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Stop daemon, nothing to do.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager();
  // No snapshots in local_state either.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  EXPECT_FALSE(manager->bridge());

  // Disable the feature.
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(
      false /* enabled */);
  LoginAsPublicSession();

  base::RunLoop run_loop;
  manager->StartLoadingSnapshot(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test success TakeSnapshot flow: when public session account logs in and MGS
// is expected to be launched, a new snapshot is expected to be taken.
TEST_F(ArcDataSnapshotdManagerBasicTest, TakeSnapshotSuccess) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  base::RunLoop run_loop;
  auto* manager = CreateManager(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);

  // Logging into public session account.
  manager->set_state_for_testing(ArcDataSnapshotdManager::State::kMgsToLaunch);

  LoginAsPublicSession();
  session_controller()->StartSession();

  ExpectStartDaemon(true /* success */);
  ExpectStartTrackingApps();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);
  // Installed 10% of tracking apps.
  apps_tracker()->update_callback().Run(10 /* percent */);
  // Need to run until idle to ensure D-Bus bridge is set up and available.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager->bridge());

  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);
  // Installed 100% of tracking apps.
  apps_tracker()->update_callback().Run(100 /* percent */);
  // Need to run until idle to ensure D-Bus bridge is set up and available.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager->bridge());

  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);

  // Expect to stop ARC.
  // Expect daemon to stop once the snapshot is taken.
  ExpectStopDaemon(true /* success */);
  // Finish ARC tracking.
  std::move(apps_tracker()->finish_callback()).Run();
  // Attempt user exit callback must be called.
  run_loop.Run();

  EXPECT_EQ(1, delegate()->stopped_callback_num());

  CheckSnapshots(1 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test that ARC data removal triggers chrome restart.
TEST_F(ArcDataSnapshotdManagerBasicTest, TakeSnapshotDataRemoval) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  base::RunLoop run_loop;
  auto* manager = CreateManager(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);

  // Logging into public session account.
  manager->set_state_for_testing(ArcDataSnapshotdManager::State::kMgsToLaunch);
  LoginAsPublicSession();
  EXPECT_TRUE(user_manager()->IsLoggedInAsPublicAccount());
  session_controller()->StartSession();

  ExpectStartTrackingApps();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);
  RequestArcDataRemoval();
  // ARC snapshot is compliant with policy.
  std::move(apps_tracker()->finish_callback()).Run();
  // Finished ARC tracking.
  run_loop.Run();

  EXPECT_EQ(1, delegate()->stopped_callback_num());
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test failure TakeSnapshot flow: when MGS has failed during TakeSnapshot flow.
TEST_F(ArcDataSnapshotdManagerBasicTest, TakeSnapshotMgsFailure) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  base::RunLoop run_loop;
  auto* manager = CreateManager(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);

  // Logging into public session account.
  manager->set_state_for_testing(ArcDataSnapshotdManager::State::kMgsToLaunch);
  LoginAsPublicSession();
  EXPECT_TRUE(user_manager()->IsLoggedInAsPublicAccount());
  session_controller()->StartSession();
  ExpectStartTrackingApps();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);

  LogoutPublicSession();
  session_controller()->StopSession(false /* success */);
  EXPECT_FALSE(user_manager()->IsLoggedInAsPublicAccount());
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);

  // Attempt user exit callback must be called.
  run_loop.Run();
  // No ARC stop callback is called, because of invalid state.
  EXPECT_EQ(0, delegate()->stopped_callback_num());
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test load snapshots flow with MGS failure.
TEST_F(ArcDataSnapshotdManagerBasicTest, OnSnapshotSessionFailedLoad) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(false /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Stop daemon, nothing to do.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager(base::DoNothing());

  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  EXPECT_FALSE(manager->bridge());

  // Start MGS with loaded snapshot..
  manager->set_state_for_testing(ArcDataSnapshotdManager::State::kRunning);
  session_controller()->StartSession();

  // MGS failure.
  session_controller()->StopSession(false /* success */);

  // Remove failed snapshot.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(1 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test take snapshots flow with MGS failure.
TEST_F(ArcDataSnapshotdManagerBasicTest, OnSnapshotSessionFailedTake) {
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Stop daemon, nothing to do.
  ExpectStopDaemon(true /* success */);
  base::RunLoop attempt_exit_run_loop;
  auto* manager = CreateManager(base::BindLambdaForTesting(
      [&attempt_exit_run_loop]() { attempt_exit_run_loop.Quit(); }));

  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager->bridge());

  // Start MGS to create a snapshot.
  manager->set_state_for_testing(ArcDataSnapshotdManager::State::kMgsToLaunch);
  session_controller()->StartSession();
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);

  // MGS failure.
  session_controller()->StopSession(false /* success */);
  attempt_exit_run_loop.Run();

  // No snapshot is generated.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test that if the snapshot update interval is not started (end time is null),
// the device is not rebooted.
TEST_F(ArcDataSnapshotdManagerBasicTest, OnSnapshotUpdateEndTimeNullFailure) {
  SetupLocalState(false /* blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager(base::DoNothing());
  manager->OnSnapshotUpdateEndTimeChanged();
  EXPECT_FALSE(manager->get_reboot_controller_for_testing());
}

// Test that if snapshot feature is not enabled, the device is not rebooted.
TEST_F(ArcDataSnapshotdManagerBasicTest,
       OnSnapshotUpdateEndTimeDisabledFailure) {
  SetupLocalState(false /* blocked_ui_mode */);
  auto* manager = CreateManager(base::DoNothing());
  manager->policy_service()->set_snapshot_update_end_time_for_testing(
      base::Time::Now());
  manager->OnSnapshotUpdateEndTimeChanged();
  EXPECT_FALSE(manager->get_reboot_controller_for_testing());
}

// Test that if both snapshots exist and no need to update them, the device is
// not rebooted.
TEST_F(ArcDataSnapshotdManagerBasicTest, OnSnapshotUpdateEndTimeExistsFailure) {
  SetupLocalState(false /* blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager(base::DoNothing());
  manager->policy_service()->set_snapshot_update_end_time_for_testing(
      base::Time::Now());
  manager->OnSnapshotUpdateEndTimeChanged();
  EXPECT_FALSE(manager->get_reboot_controller_for_testing());
}

// Test the end time changed twice in a raw scenario.
TEST_F(ArcDataSnapshotdManagerBasicTest, OnSnapshotUpdateEndTimeChanged) {
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager(base::DoNothing());
  manager->policy_service()->set_snapshot_update_end_time_for_testing(
      base::Time::Now());
  // Request reboot in blocked UI mode.
  manager->OnSnapshotUpdateEndTimeChanged();
  EXPECT_TRUE(manager->get_reboot_controller_for_testing());
  CheckSnapshots(0 /* expected_snapshots_number */,
                 true /* expected_blocked_ui_mode */);

  // The reboot is requested above.
  manager->OnSnapshotUpdateEndTimeChanged();
  EXPECT_TRUE(manager->get_reboot_controller_for_testing());
  CheckSnapshots(0 /* expected_snapshots_number */,
                 true /* expected_blocked_ui_mode */);

  // Stop requesting a reboot if not inside the snapshot update interval.
  manager->policy_service()->set_snapshot_update_end_time_for_testing(
      base::Time());
  manager->OnSnapshotUpdateEndTimeChanged();
  EXPECT_FALSE(manager->get_reboot_controller_for_testing());
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

// Test that no one state should lead to any changes except when MGS is expected
// to be launched.
TEST_P(ArcDataSnapshotdManagerStateTest, OnSnapshotSessionStarted) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager();
  manager->set_state_for_testing(expected_state());
  EXPECT_EQ(manager->state(), expected_state());
  EXPECT_FALSE(manager->bridge());
  session_controller()->StartSession();
  if (expected_state() == ArcDataSnapshotdManager::State::kMgsToLaunch)
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsLaunched);
  else
    EXPECT_EQ(manager->state(), expected_state());
}

// Test that OnSnapshotAppInstalled leads to no failure.
TEST_P(ArcDataSnapshotdManagerStateTest, OnSnapshotAppInstalled) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager();
  manager->set_state_for_testing(expected_state());
  EXPECT_EQ(manager->state(), expected_state());
  EXPECT_FALSE(manager->bridge());
  manager->OnSnapshotAppInstalled(10 /* percent */);
}

// Test that no state except kNone should lead to any changes.
TEST_P(ArcDataSnapshotdManagerStateTest, StartLoadingSnapshot) {
  // Daemon stopped in ctor, since no need to be running.
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager();
  manager->set_state_for_testing(expected_state());
  EXPECT_EQ(manager->state(), expected_state());
  EXPECT_FALSE(manager->bridge());

  base::RunLoop run_loop;
  manager->StartLoadingSnapshot(base::BindLambdaForTesting([&]() {
    EXPECT_EQ(manager->state(), expected_state());
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Test that once snapshot feature is disabled by policy, manager clears
// available snapshots and restarts the browser if in snapshot update flow.
TEST_P(ArcDataSnapshotdManagerStateTest, OnSnapshotsDisabled) {
  SetupLocalState(false /* blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  ExpectStopDaemon(false /* success */);
  base::RunLoop run_loop;
  auto* manager = CreateManager(run_loop.QuitClosure());
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  manager->set_state_for_testing(expected_state());
  EXPECT_EQ(manager->state(), expected_state());
  EXPECT_FALSE(manager->bridge());

  ExpectStartDaemon(true /* success */);
  ExpectStopDaemon(true /* success */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(
      false /* enabled */);
  manager->OnSnapshotsDisabled();

  switch (expected_state()) {
    case ArcDataSnapshotdManager::State::kBlockedUi:
    case ArcDataSnapshotdManager::State::kMgsLaunched:
    case ArcDataSnapshotdManager::State::kMgsToLaunch:
    case ArcDataSnapshotdManager::State::kLoading:
      EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kStopping);
      run_loop.Run();
      break;
    case ArcDataSnapshotdManager::State::kNone:
    case ArcDataSnapshotdManager::State::kRestored:
    case ArcDataSnapshotdManager::State::kRunning:
    case ArcDataSnapshotdManager::State::kStopping:
      EXPECT_EQ(manager->state(), expected_state());
      RunUntilIdle();
      break;
  }
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
}

TEST_P(ArcDataSnapshotdManagerStateTest, OnSnapshotUpdateEndTimeChanged) {
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);
  ExpectStopDaemon(false /* success */);
  auto* manager = CreateManager(base::DoNothing());
  manager->policy_service()->set_snapshot_update_end_time_for_testing(
      base::Time::Now());
  manager->set_state_for_testing(expected_state());
  manager->OnSnapshotUpdateEndTimeChanged();

  switch (expected_state()) {
    case ArcDataSnapshotdManager::State::kNone:
    case ArcDataSnapshotdManager::State::kLoading:
    case ArcDataSnapshotdManager::State::kRestored:
    case ArcDataSnapshotdManager::State::kRunning:
      EXPECT_TRUE(manager->get_reboot_controller_for_testing());
      CheckSnapshots(0 /* expected_snapshots_number */,
                     true /* expected_blocked_ui_mode */);
      break;
    case ArcDataSnapshotdManager::State::kBlockedUi:
    case ArcDataSnapshotdManager::State::kMgsToLaunch:
    case ArcDataSnapshotdManager::State::kMgsLaunched:
    case ArcDataSnapshotdManager::State::kStopping:
      EXPECT_FALSE(manager->get_reboot_controller_for_testing());
      CheckSnapshots(0 /* expected_snapshots_number */,
                     false /* expected_blocked_ui_mode */);
      break;
  }
}

TEST_P(ArcDataSnapshotdManagerStateTest, ExpireSnapshots) {
  SetupLocalState(false /* blocked_ui_mode */);
  ExpectStopDaemon(true /* success */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  auto* manager = CreateManager();
  manager->set_state_for_testing(expected_state());
  EXPECT_EQ(manager->state(), expected_state());
  EXPECT_FALSE(manager->bridge());

  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);

  int expected_snapshots_num;
  switch (expected_state()) {
    case ArcDataSnapshotdManager::State::kBlockedUi:
    case ArcDataSnapshotdManager::State::kMgsToLaunch:
    case ArcDataSnapshotdManager::State::kMgsLaunched:
    case ArcDataSnapshotdManager::State::kLoading:
    case ArcDataSnapshotdManager::State::kStopping:
      // Do not expire snapshots if in these states. The expectation is that
      // they are cleared during the flow or on the next session start up.
      expected_snapshots_num = 2;
      break;
    case ArcDataSnapshotdManager::State::kNone:
    case ArcDataSnapshotdManager::State::kRestored:
    case ArcDataSnapshotdManager::State::kRunning:
      // Expect snapshots to be cleared.
      ExpectStartDaemon(true /* success */);
      ExpectStopDaemon(true /* success */);
      expected_snapshots_num = 0;
      break;
  }
  ExpireSnapshots();
  CheckSnapshots(expected_snapshots_num, false /* expected_blocked_ui_mode */);
}

INSTANTIATE_TEST_SUITE_P(
    ArcDataSnapshotdManagerTest,
    ArcDataSnapshotdManagerStateTest,
    ::testing::Values(ArcDataSnapshotdManager::State::kNone,
                      ArcDataSnapshotdManager::State::kBlockedUi,
                      ArcDataSnapshotdManager::State::kLoading,
                      ArcDataSnapshotdManager::State::kMgsToLaunch,
                      ArcDataSnapshotdManager::State::kMgsLaunched,
                      ArcDataSnapshotdManager::State::kRestored,
                      ArcDataSnapshotdManager::State::kRunning,
                      ArcDataSnapshotdManager::State::kStopping));

// Test clear snapshots flow.
TEST_P(ArcDataSnapshotdManagerFlowTest, ClearSnapshotsBasic) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(false /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);

  // Once |manager| is created, it tries to clear both snapshots, because the
  // mechanism is disabled by default, and stop the daemon.
  // Start to clear snapshots.
  ExpectStartDaemon(true /* success */);
  // Stop once finished clearing.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager();
  RunUntilIdle();

  // No snapshots in local_state either.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  EXPECT_FALSE(manager->IsAutoLoginConfigured());
  EXPECT_TRUE(manager->IsAutoLoginAllowed());
  CheckSnapshots(0 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);

  EXPECT_FALSE(manager->bridge());
}

// Test blocked UI mode flow.
TEST_P(ArcDataSnapshotdManagerFlowTest, BlockedUiBasic) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(true /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */);
  // Enable snapshotting mechanism for testing.
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Once |manager| is created, it tries to clear both snapshots, because the
  // mechanism is disabled by default, and stop the daemon.
  // Start to clear snapshots.
  ExpectStartDaemon(true /* success */, {kRestartFreconEnv});
  // Stop once finished clearing.
  ExpectStopDaemon(true /* success */);
  bool is_attempt_user_exit_called = false;
  EnableHeadlessMode();
  auto* manager = CreateManager(
      base::BindLambdaForTesting([&is_attempt_user_exit_called]() {
        is_attempt_user_exit_called = true;
      }));
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kBlockedUi);
  EXPECT_TRUE(manager->IsAutoLoginConfigured());
  EXPECT_FALSE(manager->IsAutoLoginAllowed());

  RunUntilIdle();

  if (is_dbus_client_available()) {
    EXPECT_FALSE(is_attempt_user_exit_called);
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsToLaunch);
    EXPECT_TRUE(manager->IsAutoLoginConfigured());
    EXPECT_TRUE(manager->IsAutoLoginAllowed());

    EXPECT_TRUE(manager->bridge());
    // Starts a last snapshot creation. last became previous.
    CheckSnapshots(1 /* expected_snapshots_number */,
                   true /*expected_blocked_ui */,
                   true /* expected_snapshot_started */);
  } else {
    EXPECT_TRUE(is_attempt_user_exit_called);
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kBlockedUi);
    EXPECT_FALSE(manager->bridge());
    // Snapshots are valid. No need to clear.
    CheckSnapshots(2 /* expected_snapshots_number */,
                   false /* expected_blocked_ui */);
  }
}

// Test load snapshots flow.
TEST_P(ArcDataSnapshotdManagerFlowTest, LoadSnapshotsBasic) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(false /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Stop daemon, nothing to do.
  ExpectStopDaemon(true /* success */);
  auto* manager = CreateManager();
  RunUntilIdle();

  // No snapshots in local_state either.
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  CheckSnapshots(2 /* expected_snapshots_number */,
                 false /* expected_blocked_ui_mode */);
  EXPECT_FALSE(manager->bridge());

  // Loading snapshots is allowed only for public session accounts.
  LoginAsPublicSession();

  // Start daemon to load a snapshot.
  ExpectStartDaemon(true /* success */);
  ExpectStopDaemon(true /* success */);
  base::RunLoop run_loop;
  manager->StartLoadingSnapshot(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kLoading);

  run_loop.Run();
  if (is_dbus_client_available()) {
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kRunning);
    manager->OnSnapshotSessionPolicyCompliant();
    CheckVerifiedLastSnapshot();
    CheckSnapshots(2 /* expected_snapshots_number */,
                   false /* expected_blocked_ui_mode */);
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kRunning);
  } else {
    CheckSnapshots(0 /* expected_snapshots_number */,
                   false /* expected_blocked_ui_mode */);
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kNone);
  }

  // Exit MGS successfully.
  LogoutPublicSession();
}

// Test escape snapshot generating flow.
TEST_P(ArcDataSnapshotdManagerFlowTest, EscapeBasic) {
  // Set up two snapshots (previous and last) in local_state.
  SetupLocalState(true /* blocked_ui_mode */);
  CheckSnapshots(2 /* expected_snapshots_number */);
  // Enable snapshotting mechanism for testing.
  ArcDataSnapshotdManager::set_snapshot_enabled_for_testing(true /* enabled */);

  // Once |manager| is created, it tries to clear both snapshots, because the
  // mechanism is disabled by default, and stop the daemon.
  // Start to clear snapshots.
  ExpectStartDaemon(true /* success */, {kRestartFreconEnv});
  // Stop once finished clearing.
  ExpectStopDaemon(true /* success */);
  bool is_attempt_user_exit_called = false;
  EnableHeadlessMode();
  auto* manager = CreateManager(
      base::BindLambdaForTesting([&is_attempt_user_exit_called]() {
        is_attempt_user_exit_called = true;
      }));
  EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kBlockedUi);

  RunUntilIdle();

  if (is_dbus_client_available()) {
    EXPECT_FALSE(is_attempt_user_exit_called);
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kMgsToLaunch);
    EXPECT_TRUE(manager->bridge());

    // Check that connected to the cancellation signal.
    EXPECT_FALSE(client()->signal_callback().is_null());

    // Send a cancellation signal.
    client()->signal_callback().Run();
    RunUntilIdle();
    EXPECT_TRUE(is_attempt_user_exit_called);
  } else {
    EXPECT_TRUE(is_attempt_user_exit_called);
    EXPECT_EQ(manager->state(), ArcDataSnapshotdManager::State::kBlockedUi);
    EXPECT_FALSE(manager->bridge());
    // Check that not connected to the cancellation signal.
    EXPECT_TRUE(client()->signal_callback().is_null());
  }
}

INSTANTIATE_TEST_SUITE_P(ArcDataSnapshotdManagerFlowTest,
                         ArcDataSnapshotdManagerFlowTest,
                         ::testing::Values(true, false));

}  // namespace

}  // namespace data_snapshotd
}  // namespace arc
