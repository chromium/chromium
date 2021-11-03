// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/smart_lock_auth_factor_model.h"

#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

}  // namespace

class SmartLockAuthFactorModelUnittest : public AshTestBase {
 public:
  SmartLockAuthFactorModelUnittest() = default;
  SmartLockAuthFactorModelUnittest(const SmartLockAuthFactorModelUnittest&) =
      delete;
  SmartLockAuthFactorModelUnittest& operator=(
      const SmartLockAuthFactorModelUnittest&) = delete;
  ~SmartLockAuthFactorModelUnittest() override = default;

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    model_->Init(&icon_, base::BindRepeating(
                             &SmartLockAuthFactorModelUnittest::OnStateChanged,
                             base::Unretained(this)));
  }

  void OnStateChanged() { on_state_changed_called_ = true; }

  SmartLockAuthFactorModel smart_lock_model_;
  AuthFactorModel* model_ = &smart_lock_model_;
  AuthIconView icon_;
  bool on_state_changed_called_ = false;
};

TEST_F(SmartLockAuthFactorModelUnittest, GetType) {
  EXPECT_EQ(AuthFactorType::kSmartLock, model_->GetType());
}

TEST_F(SmartLockAuthFactorModelUnittest, Disabled) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kDisabled);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kUnavailable, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, ClickRequired) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kPhoneAuthenticated);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kClickRequired, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, AvailableStates) {
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

TEST_F(SmartLockAuthFactorModelUnittest, ReadyStates) {
  smart_lock_model_.SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kReady, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, OnStateChangedDebounced) {
  smart_lock_model_.SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  smart_lock_model_.SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_FALSE(on_state_changed_called_);
}

}  // namespace ash
