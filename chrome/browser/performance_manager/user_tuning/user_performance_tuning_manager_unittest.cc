// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/test_support/fake_memory_saver_mode_delegate.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {
namespace {

using MemorySaverModeState = prefs::MemorySaverModeState;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Optional;
using ::testing::ValuesIn;

}  // namespace

struct PrefTestParams {
  // State to store in the kMemorySaverModeState pref.
  MemorySaverModeState pref_state = MemorySaverModeState::kDisabled;

  // Expected state passed to ToggleMemorySaverMode().
  MemorySaverModeState expected_state = MemorySaverModeState::kDisabled;

  // Expected state passed to ToggleMemorySaverMode() when
  // ForceHeuristicMemorySaver is enabled and not ignored.
  MemorySaverModeState expected_state_with_force =
      MemorySaverModeState::kDisabled;
};

class UserPerformanceTuningManagerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<PrefTestParams> {
 protected:
  void SetUp() override {
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void RegisterDelegate() {
    auto fake_memory_saver_mode_delegate =
        std::make_unique<FakeMemorySaverModeDelegate>();

    memory_saver_mode_delegate_ = fake_memory_saver_mode_delegate.get();

    manager_.reset(new UserPerformanceTuningManager(
        &local_state_, nullptr, std::move(fake_memory_saver_mode_delegate)));
  }

  void StartManager() { manager()->Start(); }

  UserPerformanceTuningManager* manager() {
    return UserPerformanceTuningManager::GetInstance();
  }

  base::Value ValueForPrefState() const {
    return base::Value(static_cast<int>(GetParam().pref_state));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;

  raw_ptr<FakeMemorySaverModeDelegate, DanglingUntriaged>
      memory_saver_mode_delegate_;

  std::unique_ptr<UserPerformanceTuningManager> manager_;
};

class FakeTabFreezingDelegate
    : public UserPerformanceTuningManager::TabFreezingDelegate {
 public:
  void SetFreezingEnabledByUser(bool enabled) override {
    last_state_ = enabled;
  }

  std::optional<bool> GetLastState() const { return last_state_; }

 private:
  std::optional<bool> last_state_;
};

class MockUserPerformanceTuningManagerObserver
    : public UserPerformanceTuningManager::Observer {
 public:
  MOCK_METHOD(void, OnMemorySaverModeChanged, (), (override));
  MOCK_METHOD(void, OnTabFreezingModeChanged, (), (override));
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UserPerformanceTuningManagerTest,
    ::testing::Values(
        PrefTestParams{
            .pref_state = MemorySaverModeState::kDisabled,
            .expected_state = MemorySaverModeState::kDisabled,
        },
        PrefTestParams{
            .pref_state = MemorySaverModeState::kEnabled,
            .expected_state = MemorySaverModeState::kEnabled,
        }));

TEST_P(UserPerformanceTuningManagerTest, OnPrefChanged) {
  RegisterDelegate();
  StartManager();
  local_state_.SetUserPref(prefs::kMemorySaverModeState, ValueForPrefState());
  EXPECT_THAT(memory_saver_mode_delegate_->GetLastState(),
              Optional(GetParam().expected_state));
}

TEST_P(UserPerformanceTuningManagerTest, MemorySaverChangeObserver) {
  RegisterDelegate();

  std::unique_ptr<UserPerformanceTuningManager::Observer> observer =
      std::make_unique<MockUserPerformanceTuningManagerObserver>();
  auto* mock_observer =
      static_cast<MockUserPerformanceTuningManagerObserver*>(observer.get());

  // The observer shouldn't be called on startup.
  EXPECT_CALL(*mock_observer, OnMemorySaverModeChanged).Times(0);
  manager()->AddObserver(observer.get());
  StartManager();
  testing::Mock::VerifyAndClearExpectations(mock_observer);

  // The observer should be called on a subsequent pref change.
  EXPECT_CALL(*mock_observer, OnMemorySaverModeChanged).Times(1);
  manager()->SetMemorySaverModeEnabled(false);
}

class UserPerformanceTuningManagerTabFreezingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());

    auto fake_memory_saver_mode_delegate =
        std::make_unique<FakeMemorySaverModeDelegate>();
    auto fake_tab_freezing_delegate =
        std::make_unique<FakeTabFreezingDelegate>();

    tab_freezing_delegate_ = fake_tab_freezing_delegate.get();

    manager_.reset(new UserPerformanceTuningManager(
        &local_state_, nullptr, std::move(fake_memory_saver_mode_delegate),
        std::move(fake_tab_freezing_delegate)));
  }

  void TearDown() override {
    tab_freezing_delegate_ = nullptr;
    manager_.reset();
  }

  void StartManager() { manager()->Start(); }

  UserPerformanceTuningManager* manager() {
    return UserPerformanceTuningManager::GetInstance();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;

  raw_ptr<FakeTabFreezingDelegate> tab_freezing_delegate_ = nullptr;

  std::unique_ptr<UserPerformanceTuningManager> manager_;
};

TEST_F(UserPerformanceTuningManagerTabFreezingTest,
       InitialStatePassedToDelegate) {
  StartManager();
  EXPECT_THAT(tab_freezing_delegate_->GetLastState(), Optional(true));
}

TEST_F(UserPerformanceTuningManagerTabFreezingTest,
       PrefChangedNotifiesDelegateAndObserver) {
  auto observer = std::make_unique<MockUserPerformanceTuningManagerObserver>();
  manager()->AddObserver(observer.get());
  StartManager();

  EXPECT_CALL(*observer, OnTabFreezingModeChanged).Times(1);
  manager()->SetTabFreezingEnabled(false);
  EXPECT_THAT(tab_freezing_delegate_->GetLastState(), Optional(false));
  EXPECT_FALSE(manager()->IsTabFreezingActive());

  EXPECT_CALL(*observer, OnTabFreezingModeChanged).Times(1);
  manager()->SetTabFreezingEnabled(true);
  EXPECT_THAT(tab_freezing_delegate_->GetLastState(), Optional(true));
  EXPECT_TRUE(manager()->IsTabFreezingActive());

  manager()->RemoveObserver(observer.get());
}

}  // namespace performance_manager::user_tuning
