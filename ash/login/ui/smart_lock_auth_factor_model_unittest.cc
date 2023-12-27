// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/smart_lock_auth_factor_model.h"

#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

struct LabelTestcase {
  SmartLockState state;
  std::optional<bool> can_use_pin;
  int label_id;
  int accessible_name_id;
};

constexpr LabelTestcase kLabelTestcases[] = {
    {SmartLockState::kDisabled, /*can_use_pin=*/true,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED},
    {SmartLockState::kDisabled, /*can_use_pin=*/false,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED},
    {SmartLockState::kInactive, /*can_use_pin=*/true,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED},
    {SmartLockState::kInactive, /*can_use_pin=*/false,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED},
    {SmartLockState::kPrimaryUserAbsent, /*can_use_pin=*/true,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED},
    {SmartLockState::kPrimaryUserAbsent, /*can_use_pin=*/false,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED},
    {SmartLockState::kPhoneNotAuthenticated, /*can_use_pin=*/true,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED},
    {SmartLockState::kPhoneNotAuthenticated, /*can_use_pin=*/false,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED},
    {SmartLockState::kBluetoothDisabled, /*can_use_pin=*/std::nullopt,
     IDS_SMART_LOCK_LABEL_NO_BLUETOOTH, IDS_SMART_LOCK_LABEL_NO_BLUETOOTH},
    {SmartLockState::kPhoneNotLockable, /*can_use_pin=*/std::nullopt,
     IDS_SMART_LOCK_LABEL_NO_PHONE_LOCK_SCREEN,
     IDS_SMART_LOCK_LABEL_NO_PHONE_LOCK_SCREEN},
    {SmartLockState::kConnectingToPhone, /*can_use_pin=*/std::nullopt,
     IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE,
     IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE},
    {SmartLockState::kPhoneFoundLockedAndDistant, /*can_use_pin=*/std::nullopt,
     IDS_SMART_LOCK_LABEL_PHONE_TOO_FAR, IDS_SMART_LOCK_LABEL_PHONE_TOO_FAR},
    {SmartLockState::kPhoneFoundUnlockedAndDistant,
     /*can_use_pin=*/std::nullopt, IDS_SMART_LOCK_LABEL_PHONE_TOO_FAR,
     IDS_SMART_LOCK_LABEL_PHONE_TOO_FAR},
    {SmartLockState::kPhoneNotFound, /*can_use_pin=*/std::nullopt,
     IDS_SMART_LOCK_LABEL_NO_PHONE, IDS_SMART_LOCK_LABEL_NO_PHONE},
    {SmartLockState::kPhoneFoundLockedAndProximate,
     /*can_use_pin=*/std::nullopt, IDS_SMART_LOCK_LABEL_PHONE_LOCKED,
     IDS_SMART_LOCK_LABEL_PHONE_LOCKED},
    {SmartLockState::kPhoneAuthenticated, /*can_use_pin=*/std::nullopt,
     IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER,
     IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER},
};

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
  void InitializeSmartLockAuthFactorModel(
      SmartLockState initial_state = SmartLockState::kConnectingToPhone) {
    smart_lock_model_ = std::make_unique<SmartLockAuthFactorModel>(
        initial_state,
        base::BindRepeating(
            &SmartLockAuthFactorModelUnittest::ArrowButtonTapCallback,
            base::Unretained(this)));

    model_ = smart_lock_model_.get();
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

  std::unique_ptr<SmartLockAuthFactorModel> smart_lock_model_;
  raw_ptr<AuthFactorModel> model_ = nullptr;
  AuthIconView icon_;
  bool on_state_changed_called_ = false;
  bool arrow_button_tap_callback_called_ = false;
};

TEST_F(SmartLockAuthFactorModelUnittest, InitialState_ConnectingToPhone) {
  auto initial_state = SmartLockState::kConnectingToPhone;

  InitializeSmartLockAuthFactorModel(initial_state);

  // Confirm that the desired initial state was set by verifying that no
  // change occurs on a subsequent call to `SetSmartLockState()`.
  smart_lock_model_->SetSmartLockState(initial_state);
  EXPECT_FALSE(on_state_changed_called_);
}

TEST_F(SmartLockAuthFactorModelUnittest, InitialState_Inactive) {
  auto initial_state = SmartLockState::kInactive;

  InitializeSmartLockAuthFactorModel(initial_state);

  // Confirm that the desired initial state was set by verifying that no
  // change occurs on a subsequent call to `SetSmartLockState()`.
  smart_lock_model_->SetSmartLockState(initial_state);
  EXPECT_FALSE(on_state_changed_called_);
}

TEST_F(SmartLockAuthFactorModelUnittest, GetType) {
  InitializeSmartLockAuthFactorModel();
  EXPECT_EQ(AuthFactorType::kSmartLock, model_->GetType());
}

TEST_F(SmartLockAuthFactorModelUnittest, Disabled) {
  InitializeSmartLockAuthFactorModel();
  smart_lock_model_->SetSmartLockState(SmartLockState::kDisabled);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kUnavailable, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, ClickRequired) {
  InitializeSmartLockAuthFactorModel();
  smart_lock_model_->SetSmartLockState(SmartLockState::kPhoneAuthenticated);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kClickRequired, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, AvailableStates) {
  InitializeSmartLockAuthFactorModel();
  for (SmartLockState state :
       {SmartLockState::kPhoneNotFound, SmartLockState::kConnectingToPhone,
        SmartLockState::kPhoneFoundLockedAndDistant,
        SmartLockState::kPhoneFoundUnlockedAndDistant}) {
    smart_lock_model_->SetSmartLockState(state);
    EXPECT_TRUE(on_state_changed_called_);
    on_state_changed_called_ = false;
    EXPECT_EQ(AuthFactorState::kAvailable, model_->GetAuthFactorState());
  }
}

TEST_F(SmartLockAuthFactorModelUnittest, ErrorStates) {
  InitializeSmartLockAuthFactorModel();
  for (SmartLockState state : {SmartLockState::kPrimaryUserAbsent,
                               SmartLockState::kPhoneNotAuthenticated,
                               SmartLockState::kBluetoothDisabled,
                               SmartLockState::kPhoneNotLockable}) {
    smart_lock_model_->SetSmartLockState(state);
    EXPECT_TRUE(on_state_changed_called_);
    EXPECT_EQ(AuthFactorState::kErrorPermanent, model_->GetAuthFactorState());
    EXPECT_FALSE(model_->has_permanent_error_display_timed_out());
    model_->HandleErrorTimeout();
    EXPECT_TRUE(model_->has_permanent_error_display_timed_out());
    on_state_changed_called_ = false;
  }
}

TEST_F(SmartLockAuthFactorModelUnittest, ReadyStates) {
  InitializeSmartLockAuthFactorModel();
  smart_lock_model_->SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_TRUE(on_state_changed_called_);
  on_state_changed_called_ = false;
  EXPECT_EQ(AuthFactorState::kReady, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, OnStateChangedDebounced) {
  InitializeSmartLockAuthFactorModel(SmartLockState::kConnectingToPhone);
  EXPECT_FALSE(on_state_changed_called_);

  smart_lock_model_->SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_TRUE(on_state_changed_called_);

  on_state_changed_called_ = false;
  smart_lock_model_->SetSmartLockState(
      SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_FALSE(on_state_changed_called_);
}

TEST_F(SmartLockAuthFactorModelUnittest, ArrowButtonTapCallback) {
  InitializeSmartLockAuthFactorModel();
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
  TestArrowButtonAndCheckCallbackCalled(SmartLockState::kPrimaryUserAbsent,
                                        false);
}

TEST_F(SmartLockAuthFactorModelUnittest, NotifySmartLockAuthResult) {
  InitializeSmartLockAuthFactorModel();
  smart_lock_model_->NotifySmartLockAuthResult(/*result=*/true);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kAuthenticated, model_->GetAuthFactorState());

  on_state_changed_called_ = false;
  smart_lock_model_->NotifySmartLockAuthResult(/*result=*/false);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kErrorPermanent, model_->GetAuthFactorState());
}

TEST_F(SmartLockAuthFactorModelUnittest, GetLabelAndAccessibleName) {
  InitializeSmartLockAuthFactorModel();
  for (const LabelTestcase& testcase : kLabelTestcases) {
    smart_lock_model_->SetSmartLockState(testcase.state);
    if (testcase.can_use_pin.has_value()) {
      model_->set_can_use_pin(testcase.can_use_pin.value());
    }
    EXPECT_EQ(testcase.label_id, model_->GetLabelId());
    EXPECT_EQ(testcase.accessible_name_id, model_->GetAccessibleNameId());
    EXPECT_TRUE(model_->ShouldAnnounceLabel());
  }
}

TEST_F(SmartLockAuthFactorModelUnittest, GetLabelAfterPermanentErrorTimeout) {
  InitializeSmartLockAuthFactorModel();
  smart_lock_model_->NotifySmartLockAuthResult(/*result=*/false);
  EXPECT_TRUE(on_state_changed_called_);
  EXPECT_EQ(AuthFactorState::kErrorPermanent, model_->GetAuthFactorState());
  EXPECT_EQ(IDS_AUTH_FACTOR_LABEL_CANNOT_UNLOCK, model_->GetLabelId());
  model_->HandleErrorTimeout();
  EXPECT_EQ(IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED, model_->GetLabelId());
}

}  // namespace ash
