// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/snapshot_session_controller.h"

#include <memory>
#include <string>

#include "ash/components/arc/test/fake_apps_tracker.h"
#include "base/test/task_environment.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kPublicAccountEmail[] = "public-session-account@localhost";

class FakeObserver final : public SnapshotSessionController::Observer {
 public:
  FakeObserver() = default;
  FakeObserver(const FakeObserver&) = delete;
  FakeObserver& operator=(const FakeObserver&) = delete;
  ~FakeObserver() override = default;

  void OnSnapshotSessionStarted() override { session_started_num_++; }

  void OnSnapshotSessionStopped() override { session_stopped_num_++; }

  void OnSnapshotSessionFailed() override { session_failed_num_++; }

  void OnSnapshotAppInstalled(int percent) override {
    apps_installed_percent_ = percent;
  }

  void OnSnapshotSessionPolicyCompliant() override {
    session_policy_compliant_num_++;
  }

  int session_started_num() const { return session_started_num_; }
  int session_stopped_num() const { return session_stopped_num_; }
  int session_failed_num() const { return session_failed_num_; }
  int apps_installed_percent() const { return apps_installed_percent_; }
  int session_policy_compliant_num() const {
    return session_policy_compliant_num_;
  }

 private:
  int session_started_num_ = 0;
  int session_stopped_num_ = 0;
  int session_failed_num_ = 0;
  int apps_installed_percent_ = -1;
  int session_policy_compliant_num_ = 0;
};

}  // namespace

// Tests SnapshotSessionController class instance.
class SnapshotSessionControllerTest : public testing::Test {
 protected:
  SnapshotSessionControllerTest() = default;

  void SetUp() override {
    fake_user_manager_ = new user_manager::FakeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
    observer_ = std::make_unique<FakeObserver>();
    session_manager_.SetSessionState(session_manager::SessionState::UNKNOWN);
  }

  void LoginAsPublicSession() {
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    auto* user = user_manager()->AddPublicAccountUser(account_id);
    user_manager()->UserLoggedIn(account_id, user->username_hash(), false,
                                 false);
  }

  void SetSessionState(session_manager::SessionState state) {
    session_manager_.SetSessionState(state);
  }

  void LogoutPublicSession() {
    user_manager()->LogoutAllUsers();
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    user_manager()->RemoveUserFromList(account_id);
    session_manager_.SetSessionState(session_manager::SessionState::LOCKED);
  }

  std::unique_ptr<FakeAppsTracker> CreateAppsTracker() {
    auto apps_tracker = std::make_unique<FakeAppsTracker>();
    apps_tracker_ = apps_tracker.get();

    return apps_tracker;
  }

  user_manager::FakeUserManager* user_manager() { return fake_user_manager_; }
  FakeAppsTracker* apps_tracker() { return apps_tracker_; }
  FakeObserver* observer() { return observer_.get(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  session_manager::SessionManager session_manager_;
  user_manager::FakeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  FakeAppsTracker* apps_tracker_;
  std::unique_ptr<FakeObserver> observer_;
};

TEST_F(SnapshotSessionControllerTest, BasicPreLogin) {
  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::ACTIVE);
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());

  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());
}

TEST_F(SnapshotSessionControllerTest, BasicNoLogin) {
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());

  EXPECT_FALSE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(0, apps_tracker()->start_tracking_num());
}

TEST_F(SnapshotSessionControllerTest, StartSession) {
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());
  session_controller->AddObserver(observer());

  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());
  EXPECT_EQ(1, observer()->session_started_num());

  session_controller->RemoveObserver(observer());
}

TEST_F(SnapshotSessionControllerTest, StopSessionFailure) {
  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::ACTIVE);
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());
  session_controller->AddObserver(observer());

  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());

  LogoutPublicSession();
  EXPECT_EQ(1, observer()->session_failed_num());
  EXPECT_FALSE(session_controller->get_timer_for_testing()->IsRunning());

  session_controller->RemoveObserver(observer());
}

TEST_F(SnapshotSessionControllerTest, StopSessionSuccess) {
  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::ACTIVE);
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());
  session_controller->AddObserver(observer());

  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());
  apps_tracker()->update_callback().Run(100 /* percent */);

  apps_tracker()->finish_callback().Run();
  EXPECT_EQ(1, observer()->session_policy_compliant_num());

  LogoutPublicSession();
  EXPECT_EQ(1, observer()->session_stopped_num());
  EXPECT_FALSE(session_controller->get_timer_for_testing()->IsRunning());

  session_controller->RemoveObserver(observer());
}

TEST_F(SnapshotSessionControllerTest, OnAppInstalled) {
  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::ACTIVE);
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());
  session_controller->AddObserver(observer());

  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());

  apps_tracker()->update_callback().Run(10 /* percent */);
  EXPECT_EQ(10, observer()->apps_installed_percent());
  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());

  session_controller->RemoveObserver(observer());
}

TEST_F(SnapshotSessionControllerTest, StopSessionFailureDuration) {
  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::ACTIVE);
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());
  session_controller->AddObserver(observer());

  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());

  task_environment_.FastForwardBy(base::Minutes(40));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, observer()->session_failed_num());

  session_controller->RemoveObserver(observer());
}

TEST_F(SnapshotSessionControllerTest, DoubleStartSession) {
  auto session_controller =
      SnapshotSessionController::Create(CreateAppsTracker());
  session_controller->AddObserver(observer());

  LoginAsPublicSession();
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(0, apps_tracker()->start_tracking_num());
  EXPECT_EQ(0, observer()->session_started_num());

  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(session_controller->get_timer_for_testing()->IsRunning());
  EXPECT_EQ(1, apps_tracker()->start_tracking_num());
  EXPECT_EQ(1, observer()->session_started_num());

  session_controller->RemoveObserver(observer());
}

}  // namespace data_snapshotd
}  // namespace arc
