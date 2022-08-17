// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/user_tuning/fake_frame_throttling_delegate.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {

namespace {

class FakeHighEfficiencyModeToggleDelegate
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeToggleDelegate {
 public:
  void ToggleHighEfficiencyMode(bool enabled) override {}
  ~FakeHighEfficiencyModeToggleDelegate() override = default;
};

}  // namespace

class UserPerformanceTuningManagerTest : public testing::Test {
 public:
  void SetUp() override {
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void StartManager(
      std::vector<base::test::ScopedFeatureList::FeatureAndParams>
          features_and_params = {
              {performance_manager::features::kBatterySaverModeAvailable, {}},
              {performance_manager::features::kHighEfficiencyModeAvailable, {}},
          }) {
    feature_list_.InitWithFeaturesAndParameters(features_and_params, {});
    manager_.reset(new UserPerformanceTuningManager(
        &local_state_,
        std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled_),
        std::make_unique<FakeHighEfficiencyModeToggleDelegate>()));
    manager()->Start();
  }

  UserPerformanceTuningManager* manager() {
    return UserPerformanceTuningManager::GetInstance();
  }
  bool throttling_enabled() const { return throttling_enabled_; }

  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;

  bool throttling_enabled_ = false;
  std::unique_ptr<UserPerformanceTuningManager> manager_;
};

TEST_F(UserPerformanceTuningManagerTest, TemporaryBatterySaver) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(false);
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  // Changing the pref resets the "disabled for session" flag.
  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_FALSE(manager()->IsBatterySaverModeDisabledForSession());

  // TODO(anthonyvd): Test the flag is cleared when the device is plugged in
  // once that CL lands.
}

TEST_F(UserPerformanceTuningManagerTest, BatterySaverModePref) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
}

TEST_F(UserPerformanceTuningManagerTest, InvalidPrefInStore) {
  StartManager();
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState, -1);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled) +
          1);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
}

TEST_F(UserPerformanceTuningManagerTest, HEMFinchDisabledByDefault) {
  StartManager({
      {performance_manager::features::kHighEfficiencyModeAvailable,
       {{"default_state", "false"}}},
  });

  EXPECT_FALSE(local_state_.GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled));
}

TEST_F(UserPerformanceTuningManagerTest, HEMFinchEnabledByDefault) {
  StartManager({
      {performance_manager::features::kHighEfficiencyModeAvailable,
       {{"default_state", "true"}}},
  });

  EXPECT_TRUE(local_state_.GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled));
}

}  // namespace performance_manager::user_tuning
