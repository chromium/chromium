// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_state_manager.h"

#include "ash/constants/ash_features.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
class MockFjordOobeStateManagerObserver
    : public FjordOobeStateManager::Observer {
 public:
  MOCK_METHOD(void,
              OnFjordOobeStateChanged,
              (fjord_oobe_state::proto::FjordOobeStateInfo new_state));
};

fjord_oobe_state::proto::FjordOobeStateInfo CreateFjordOobeStateInfo(
    fjord_oobe_state::proto::FjordOobeStateInfo::FjordOobeState state) {
  fjord_oobe_state::proto::FjordOobeStateInfo state_info;
  state_info.set_oobe_state(state);
  return state_info;
}
}  // namespace

class FjordOobeStateManagerTest : public testing::Test {
 public:
  FjordOobeStateManagerTest() {
    feature_list_.InitAndEnableFeature(features::kFjordOobeForceEnabled);
    FjordOobeStateManager::Initialize();

    mock_observer_ = std::make_unique<MockFjordOobeStateManagerObserver>();
    FjordOobeStateManager::Get()->AddObserver(mock_observer_.get());
  }

  ~FjordOobeStateManagerTest() override { FjordOobeStateManager::Shutdown(); }

 protected:
  std::unique_ptr<MockFjordOobeStateManagerObserver> mock_observer_;

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

  fjord_oobe_state::proto::FjordOobeStateInfo expected_proto =
      CreateFjordOobeStateInfo(
          fjord_oobe_state::proto::FjordOobeStateInfo::FJORD_OOBE_STATE_START);
  EXPECT_CALL(*mock_observer_,
              OnFjordOobeStateChanged(base::test::EqualsProto(expected_proto)))
      .Times(1);

  manager->SetFjordOobeState(
      fjord_oobe_state::proto::FjordOobeStateInfo::FJORD_OOBE_STATE_START);

  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            expected_proto.oobe_state());
}

class FjordOobeStateManagerFeatureDisabledTest : public testing::Test {
 public:
  FjordOobeStateManagerFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(features::kFjordOobeForceEnabled);
    FjordOobeStateManager::Initialize();

    mock_observer_ = std::make_unique<MockFjordOobeStateManagerObserver>();
    FjordOobeStateManager::Get()->AddObserver(mock_observer_.get());
  }

  ~FjordOobeStateManagerFeatureDisabledTest() override {
    FjordOobeStateManager::Shutdown();
  }

 protected:
  std::unique_ptr<MockFjordOobeStateManagerObserver> mock_observer_;

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

  EXPECT_CALL(*mock_observer_, OnFjordOobeStateChanged(testing::_)).Times(0);

  manager->SetFjordOobeState(
      fjord_oobe_state::proto::FjordOobeStateInfo::FJORD_OOBE_STATE_START);

  EXPECT_EQ(manager->GetFjordOobeStateInfo().oobe_state(),
            fjord_oobe_state::proto::FjordOobeStateInfo::
                FJORD_OOBE_STATE_UNIMPLEMENTED);
}
}  // namespace ash
