// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/usage_time_state_notifier.h"

#include <memory>
#include <vector>

#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestUsageTimeStateNotifierObserver
    : public UsageTimeStateNotifier::Observer {
 public:
  TestUsageTimeStateNotifierObserver() = default;

  TestUsageTimeStateNotifierObserver(
      const TestUsageTimeStateNotifierObserver&) = delete;
  TestUsageTimeStateNotifierObserver& operator=(
      const TestUsageTimeStateNotifierObserver&) = delete;

  ~TestUsageTimeStateNotifierObserver() override = default;

  const std::vector<UsageTimeStateNotifier::UsageTimeState>& events() const {
    return usage_time_state_changes_;
  }

 private:
  void OnUsageTimeStateChange(
      const UsageTimeStateNotifier::UsageTimeState state) override {
    usage_time_state_changes_.push_back(state);
  }

  std::vector<UsageTimeStateNotifier::UsageTimeState> usage_time_state_changes_;
};

}  // namespace

class UsageTimeStateNotifierTest : public testing::Test {
 protected:
  UsageTimeStateNotifierTest() = default;

  UsageTimeStateNotifierTest(const UsageTimeStateNotifierTest&) = delete;
  UsageTimeStateNotifierTest& operator=(const UsageTimeStateNotifierTest&) =
      delete;

  ~UsageTimeStateNotifierTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }

  void TearDown() override { chromeos::PowerManagerClient::Shutdown(); }

  void NotifyScreenIdleOffChanged(bool off) {
    power_manager::ScreenIdleState proto;
    proto.set_off(off);
    power_manager_client()->SendScreenIdleStateChanged(proto);
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

 private:
  session_manager::SessionManager session_manager_;
};

TEST_F(UsageTimeStateNotifierTest, CallObserverWhenSessionIsActive) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, CallObserverWhenSessionIsLocked) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE,
      UsageTimeStateNotifier::UsageTimeState::INACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, CallObserverWhenDeviceIsSuspend) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE,
      UsageTimeStateNotifier::UsageTimeState::INACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, CallObserverWhenDeviceHasFinishedSuspend) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  power_manager_client()->SendSuspendDone();
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE,
      UsageTimeStateNotifier::UsageTimeState::INACTIVE,
      UsageTimeStateNotifier::UsageTimeState::ACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, CallObserversWhenScreenIsTurnedOff) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  NotifyScreenIdleOffChanged(true);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE,
      UsageTimeStateNotifier::UsageTimeState::INACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, CallObserversWhenScreenIsTurnedOn) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  NotifyScreenIdleOffChanged(true);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  NotifyScreenIdleOffChanged(false);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE,
      UsageTimeStateNotifier::UsageTimeState::INACTIVE,
      UsageTimeStateNotifier::UsageTimeState::ACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, DoNotCallObserversWhenStateDoNotChange) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  NotifyScreenIdleOffChanged(false);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());

  std::vector<UsageTimeStateNotifier::UsageTimeState> expected = {
      UsageTimeStateNotifier::UsageTimeState::ACTIVE};

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

TEST_F(UsageTimeStateNotifierTest, CallObserversForMultipleEvents) {
  TestUsageTimeStateNotifierObserver observer;
  UsageTimeStateNotifier::GetInstance()->AddObserver(&observer);
  std::vector<UsageTimeStateNotifier::UsageTimeState> expected;

  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::ACTIVE);

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::INACTIVE);

  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::ACTIVE);

  NotifyScreenIdleOffChanged(true);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::INACTIVE);

  NotifyScreenIdleOffChanged(false);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::ACTIVE);

  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::INACTIVE);

  power_manager_client()->SendSuspendDone();
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::INACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(UsageTimeStateNotifier::UsageTimeState::ACTIVE,
            UsageTimeStateNotifier::GetInstance()->GetState());
  expected.push_back(UsageTimeStateNotifier::UsageTimeState::ACTIVE);

  EXPECT_EQ(expected, observer.events());
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(&observer);
}

}  // namespace ash
