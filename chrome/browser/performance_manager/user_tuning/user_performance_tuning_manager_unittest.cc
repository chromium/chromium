// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_manager.h"

#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {

class FakeFrameThrottlingDelegate
    : public UserPerformanceTuningManager::FrameThrottlingDelegate {
 public:
  void StartThrottlingAllFrameSinks() override { *throttling_enabled_ = true; }
  void StopThrottlingAllFrameSinks() override { *throttling_enabled_ = false; }

  explicit FakeFrameThrottlingDelegate(bool* throttling_enabled)
      : throttling_enabled_(throttling_enabled) {}
  ~FakeFrameThrottlingDelegate() override = default;

  bool* throttling_enabled_;
};

class UserPerformanceTuningManagerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kBatterySaverModeAvailable);

    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UserPerformanceTuningManagerTest, TemporaryBatterySaver) {
  bool throttling_enabled = false;
  UserPerformanceTuningManager manager(
      &local_state_,
      std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled));
  EXPECT_FALSE(manager.IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled);

  manager.SetTemporaryBatterySaver(true);
  EXPECT_TRUE(manager.IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled);

  manager.SetTemporaryBatterySaver(false);
  EXPECT_FALSE(manager.IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled);
}

TEST_F(UserPerformanceTuningManagerTest, BatterySaverModePref) {
  bool throttling_enabled = false;
  UserPerformanceTuningManager manager(
      &local_state_,
      std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled));
  EXPECT_FALSE(manager.IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled);

  local_state_.SetBoolean(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled, true);
  EXPECT_TRUE(manager.IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled);

  local_state_.SetBoolean(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled, false);
  EXPECT_FALSE(manager.IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled);
}

TEST_F(UserPerformanceTuningManagerTest, PrefSupersedesTemporary) {
  bool throttling_enabled = false;
  UserPerformanceTuningManager manager(
      &local_state_,
      std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled));

  local_state_.SetBoolean(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled, true);
  EXPECT_TRUE(manager.IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled);

  manager.SetTemporaryBatterySaver(true);
  EXPECT_TRUE(manager.IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled);

  manager.SetTemporaryBatterySaver(false);
  EXPECT_TRUE(manager.IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled);
}

}  // namespace performance_manager::user_tuning
