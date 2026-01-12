// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_state_manager.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class FjordOobeStateManagerTest : public testing::Test {
 public:
  FjordOobeStateManagerTest() {
    feature_list_.InitAndEnableFeature(features::kFjordOobeForceEnabled);
    FjordOobeStateManager::Initialize();
  }

  ~FjordOobeStateManagerTest() override { FjordOobeStateManager::Shutdown(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FjordOobeStateManagerTest, InitialGetOobeStateReturnsUnspecified) {
  FjordOobeStateManager* manager = FjordOobeStateManager::Get();

  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            fjord_oobe_state::proto::FjordOobeStateInfo::
                FJORD_OOBE_STATE_UNSPECIFIED);
}

TEST_F(FjordOobeStateManagerTest, SetOobeStateUpdatesState) {
  FjordOobeStateManager* manager = FjordOobeStateManager::Get();
  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            fjord_oobe_state::proto::FjordOobeStateInfo::
                FJORD_OOBE_STATE_UNSPECIFIED);

  manager->OnFjordOobeStateChanged(
      fjord_oobe_state::proto::FjordOobeStateInfo::FJORD_OOBE_STATE_START);

  EXPECT_EQ(
      manager->GetFjordOobeStateInfo().oobe_state(),
      fjord_oobe_state::proto::FjordOobeStateInfo::FJORD_OOBE_STATE_START);
}

class FjordOobeStateManagerFeatureDisabledTest : public testing::Test {
 public:
  FjordOobeStateManagerFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(features::kFjordOobeForceEnabled);
    FjordOobeStateManager::Initialize();
  }

  ~FjordOobeStateManagerFeatureDisabledTest() override {
    FjordOobeStateManager::Shutdown();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FjordOobeStateManagerFeatureDisabledTest,
       GetOobeStateReturnsUnimplemented) {
  FjordOobeStateManager* manager = FjordOobeStateManager::Get();

  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            fjord_oobe_state::proto::FjordOobeStateInfo::
                FJORD_OOBE_STATE_UNIMPLEMENTED);
}

TEST_F(FjordOobeStateManagerFeatureDisabledTest,
       SetOobeStateDoesNotChangeState) {
  FjordOobeStateManager* manager = FjordOobeStateManager::Get();
  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            fjord_oobe_state::proto::FjordOobeStateInfo::
                FJORD_OOBE_STATE_UNIMPLEMENTED);

  manager->OnFjordOobeStateChanged(
      fjord_oobe_state::proto::FjordOobeStateInfo::FJORD_OOBE_STATE_START);

  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            fjord_oobe_state::proto::FjordOobeStateInfo::
                FJORD_OOBE_STATE_UNIMPLEMENTED);
}
}  // namespace ash
