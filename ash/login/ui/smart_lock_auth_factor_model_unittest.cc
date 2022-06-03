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
  void ArrowButtonTapCallback() { arrow_button_tap_callback_called_ = true; }

  void TestArrowButtonAndCheckCallbackCalled(SmartLockState state,
                                             bool should_callback_be_called) {
    arrow_button_tap_callback_called_ = false;
    smart_lock_model_->SetSmartLockState(state);
    smart_lock_model_->OnArrowButtonTapOrClickEvent();
    EXPECT_EQ(arrow_button_tap_callback_called_, should_callback_be_called);
  }

  std::unique_ptr<SmartLockAuthFactorModel> smart_lock_model_ =
      std::make_unique<SmartLockAuthFactorModel>(base::BindRepeating(
          &SmartLockAuthFactorModelUnittest::ArrowButtonTapCallback,
          base::Unretained(this)));
  AuthFactorModel* model_ = smart_lock_model_.get();
  AuthIconView icon_;
  bool on_state_changed_called_ = false;
  bool arrow_button_tap_callback_called_ = false;
};

TEST_F(SmartLockAuthFactorModelUnittest, GetType) {
  EXPECT_EQ(AuthFactorType::kSmartLock, model_->GetType());
}

TEST_F(SmartLockAuthFactorModelUnittest, Disabled) {
  smart_lock_model_->SetSmartLockState(SmartLockState::kDisabled);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kUnavailable, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, ClickRequired) {
  smart_lock_model_->SetSmartLockState(SmartLockState::kPhoneAuthenticated);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kClickRequired, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, AvailableStates) {
  smart_lock_model_->SetSmartLockState(SmartLockState::kPhoneNotFound);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());

  smart_lock_model_->SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());

  smart_lock_model_->SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndDistant);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());

  smart_lock_model_->SetSmartLockState(
      SmartLockState::kPhoneFoundUnlockedAndDistant);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, ReadyStates) {
  smart_lock_model_->SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kReady, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, OnStateChangedDebounced) {
  smart_lock_model_->SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  smart_lock_model_->SetSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_FALSE(on_state_changed_called_);
}

TEST_F(SmartLockAuthFactorModelUnittest, ArrowButtonTapCallback) {
  // Callback should only be called when state is
  // SmartLockState::kPhoneAuthenticated
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kDisabled, false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kInactive, false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kBluetoothDisabled,
                                        false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kPhoneNotLockable,
                                        false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kPhoneNotFound, false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kConnectingToPhone,
                                        false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kPhoneNotAuthenticated,
                                        false);
  TestArrowButtonAndCheckCallbackCalled(
      SmartLockState::kPhoneFoundLockedAndDistant, false);
  TestArrowButtonAndCheckCallbackCalled(
      SmartLockState::kPhoneFoundLockedAndProximate, false);
  TestArrowButtonAndCheckCallbackCalled(
      SmartLockState::kPhoneFoundUnlockedAndDistant, false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kPhoneAuthenticated,
                                        true);
  TestArrowButtonAndCheckCallbackCalled(
      SmartLockState::kPasswordReentryRequired, false);
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kPrimaryUserAbsent,
                                        false);
}

}  // namespace ash
