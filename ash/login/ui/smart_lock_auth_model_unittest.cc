// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/smart_lock_auth_model.h"

#include "ash/login/ui/auth_factor_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

}  // namespace

class SmartLockAuthModelUnittest : public testing::Test {
 public:
  SmartLockAuthModelUnittest() = default;
  SmartLockAuthModelUnittest(const SmartLockAuthModelUnittest&) = delete;
  SmartLockAuthModelUnittest& operator=(const SmartLockAuthModelUnittest&) =
      delete;
  ~SmartLockAuthModelUnittest() override = default;

 protected:
  // test::Test:
  void SetUp() override {
    model_->SetOnStateChangedCallback(base::BindRepeating(
        &SmartLockAuthModelUnittest::OnStateChanged, base::Unretained(this)));
  }

  void OnStateChanged() { on_state_changed_called_ = true; }

  SmartLockAuthModel smart_lock_model_;
  AuthFactorModel* model_ = &smart_lock_model_;
  bool on_state_changed_called_ = false;
};

TEST_F(SmartLockAuthModelUnittest, GetType) {
  EXPECT_EQ(AuthFactorType::kSmartLock, model_->GetType());
}

TEST_F(SmartLockAuthModelUnittest, Disabled) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kDisabled);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kUnavailable, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthModelUnittest, ClickRequired) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kPhoneAuthenticated);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kClickRequired, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthModelUnittest, AvailableStates) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kPhoneNotFound);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());

  smart_lock_model_.SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());

  smart_lock_model_.SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndDistant);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());

  smart_lock_model_.SetSmartLockState(
      SmartLockState::kPhoneFoundUnlockedAndDistant);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthModelUnittest, ReadyStates) {
  smart_lock_model_.SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kReady, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthModelUnittest, OnStateChangedDebounced) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  smart_lock_model_.SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_FALSE(on_state_changed_called_);
}

}  // namespace ash
