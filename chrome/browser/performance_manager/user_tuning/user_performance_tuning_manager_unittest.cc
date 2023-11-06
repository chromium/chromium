// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/test_support/fake_high_efficiency_mode_delegate.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {
namespace {

using HighEfficiencyModeState = prefs::HighEfficiencyModeState;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Optional;
using ::testing::ValuesIn;

}  // namespace

struct PrefTestParams {
  // State to store in the kHighEfficiencyModeState pref.
  HighEfficiencyModeState pref_state = HighEfficiencyModeState::kDisabled;

  // Expected state passed to ToggleHighEfficiencyMode().
  HighEfficiencyModeState expected_state = HighEfficiencyModeState::kDisabled;

  // Expected state passed to ToggleHighEfficiencyMode() when
  // ForceHeuristicMemorySaver is enabled and not ignored.
  HighEfficiencyModeState expected_state_with_force =
      HighEfficiencyModeState::kDisabled;
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
    auto fake_high_efficiency_mode_delegate =
        std::make_unique<FakeHighEfficiencyModeDelegate>();

    high_efficiency_mode_delegate_ = fake_high_efficiency_mode_delegate.get();

    manager_.reset(new UserPerformanceTuningManager(
        &local_state_, nullptr, std::move(fake_high_efficiency_mode_delegate)));
  }

  void StartManager() { manager()->Start(); }

  UserPerformanceTuningManager* manager() {
    return UserPerformanceTuningManager::GetInstance();
  }

  void InstallFeatures(bool is_multistate_enabled = false) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (is_multistate_enabled) {
      enabled_features.push_back(features::kHighEfficiencyMultistateMode);
    } else {
      disabled_features.push_back(features::kHighEfficiencyMultistateMode);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  base::Value ValueForPrefState() const {
    return base::Value(static_cast<int>(GetParam().pref_state));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;

  raw_ptr<FakeHighEfficiencyModeDelegate, DanglingUntriaged>
      high_efficiency_mode_delegate_;

  std::unique_ptr<UserPerformanceTuningManager> manager_;

  base::test::ScopedFeatureList feature_list_;
};

class MockUserPerformanceTuningManagerObserver
    : public UserPerformanceTuningManager::Observer {
 public:
  MOCK_METHOD(void, OnHighEfficiencyModeChanged, (), (override));
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UserPerformanceTuningManagerTest,
    ::testing::Values(
        PrefTestParams{
            .pref_state = HighEfficiencyModeState::kDisabled,
            .expected_state = HighEfficiencyModeState::kDisabled,
        },
        PrefTestParams{
            .pref_state = HighEfficiencyModeState::kEnabled,
            .expected_state = HighEfficiencyModeState::kEnabledOnTimer,
        },
        PrefTestParams{
            .pref_state = HighEfficiencyModeState::kEnabledOnTimer,
            .expected_state = HighEfficiencyModeState::kEnabledOnTimer,
        }));

TEST_P(UserPerformanceTuningManagerTest, OnPrefChanged) {
  InstallFeatures();
  RegisterDelegate();
  StartManager();
  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().expected_state));
}

TEST_P(UserPerformanceTuningManagerTest, OnPrefChangedMultistate) {
  InstallFeatures(/*is_multistate_enabled=*/true);
  RegisterDelegate();
  StartManager();

  // When the HighEfficiencyMultistateMode feature is enabled, all states should
  // be passed to ToggleHighEfficiencyMode() unchanged.
  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().pref_state));
}

TEST_F(UserPerformanceTuningManagerTest, HighEfficiencyChangeObserver) {
  InstallFeatures();
  RegisterDelegate();

  std::unique_ptr<UserPerformanceTuningManager::Observer> observer =
      std::make_unique<MockUserPerformanceTuningManagerObserver>();
  auto* mock_observer =
      static_cast<MockUserPerformanceTuningManagerObserver*>(observer.get());

  // The observer shouldn't be called on startup.
  EXPECT_CALL(*mock_observer, OnHighEfficiencyModeChanged).Times(0);
  manager()->AddObserver(observer.get());
  StartManager();
  testing::Mock::VerifyAndClearExpectations(mock_observer);

  // The observer should be called on a subsequent pref change.
  EXPECT_CALL(*mock_observer, OnHighEfficiencyModeChanged).Times(1);
  manager()->SetHighEfficiencyModeEnabled(false);
}

}  // namespace performance_manager::user_tuning
