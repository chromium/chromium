// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/smart_lock_auth_factor_model.h"

#include "ash/login/ui/auth_icon_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

// static
SmartLockAuthFactorModel::Factory*
    SmartLockAuthFactorModel::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<SmartLockAuthFactorModel>
SmartLockAuthFactorModel::Factory::Create(
    SmartLockState initial_state,
    base::RepeatingCallback<void()> arrow_button_tap_callback) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(initial_state,
                                             arrow_button_tap_callback);
  }
  return std::make_unique<SmartLockAuthFactorModel>(initial_state,
                                                    arrow_button_tap_callback);
}

// static
void SmartLockAuthFactorModel::Factory::SetFactoryForTesting(
    SmartLockAuthFactorModel::Factory* factory) {
  factory_instance_ = factory;
}

SmartLockAuthFactorModel::SmartLockAuthFactorModel(
    SmartLockState initial_state,
    base::RepeatingCallback<void()> arrow_button_tap_callback)
    : state_(initial_state),
      arrow_button_tap_callback_(arrow_button_tap_callback) {}

SmartLockAuthFactorModel::~SmartLockAuthFactorModel() = default;

void SmartLockAuthFactorModel::OnArrowButtonTapOrClickEvent() {
  if (state_ == SmartLockState::kPhoneAuthenticated) {
    arrow_button_tap_callback_.Run();
  }
}

void SmartLockAuthFactorModel::SetSmartLockState(SmartLockState state) {
  if (state_ == state) {
    return;
  }

  // Clear out the timeout if the state changes. This shouldn't happen
  // ordinarily -- permanent error states are permanent after all -- but this is
  // required for the debug overlay to work properly when cycling states.
  has_permanent_error_display_timed_out_ = false;

  state_ = state;
  RefreshUI();
}

void SmartLockAuthFactorModel::NotifySmartLockAuthResult(bool result) {
  auth_result_ = result;
  RefreshUI();
}

AuthFactorModel::AuthFactorState SmartLockAuthFactorModel::GetAuthFactorState()
    const {
  if (auth_result_.has_value()) {
    return auth_result_.value() ? AuthFactorState::kAuthenticated
                                : AuthFactorState::kErrorPermanent;
  }

  switch (state_) {
    case SmartLockState::kDisabled:
      [[fallthrough]];
    case SmartLockState::kInactive:
      return AuthFactorState::kUnavailable;
    case SmartLockState::kPhoneNotFound:
      [[fallthrough]];
    case SmartLockState::kConnectingToPhone:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundLockedAndDistant:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      return AuthFactorState::kAvailable;
    case SmartLockState::kPhoneAuthenticated:
      return AuthFactorState::kClickRequired;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      return AuthFactorState::kReady;
    case SmartLockState::kPrimaryUserAbsent:
      [[fallthrough]];
    case SmartLockState::kPhoneNotAuthenticated:
      [[fallthrough]];
    case SmartLockState::kBluetoothDisabled:
      [[fallthrough]];
    case SmartLockState::kPhoneNotLockable:
      return AuthFactorState::kErrorPermanent;
  }
}

AuthFactorType SmartLockAuthFactorModel::GetType() const {
  return AuthFactorType::kSmartLock;
}

int SmartLockAuthFactorModel::GetLabelId() const {
  if (auth_result_.has_value()) {
    if (auth_result_.value()) {
      return IDS_AUTH_FACTOR_LABEL_UNLOCKED;
    }

    // Once the Smart Lock error message has timed out, prompt the
    // user to enter their password (since Smart Lock has permanently failed).
    return has_permanent_error_display_timed_out_
               ? IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED
               : IDS_AUTH_FACTOR_LABEL_CANNOT_UNLOCK;
  }

  switch (state_) {
    case SmartLockState::kDisabled:
      [[fallthrough]];
    case SmartLockState::kInactive:
      [[fallthrough]];
    case SmartLockState::kPrimaryUserAbsent:
      [[fallthrough]];
    case SmartLockState::kPhoneNotAuthenticated:
      return can_use_pin_ ? IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED
                          : IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED;
    case SmartLockState::kBluetoothDisabled:
      return IDS_SMART_LOCK_LABEL_NO_BLUETOOTH;
    case SmartLockState::kPhoneNotLockable:
      return IDS_SMART_LOCK_LABEL_NO_PHONE_LOCK_SCREEN;
    case SmartLockState::kConnectingToPhone:
      return IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      return IDS_SMART_LOCK_LABEL_PHONE_TOO_FAR;
    case SmartLockState::kPhoneNotFound:
      return IDS_SMART_LOCK_LABEL_NO_PHONE;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      return IDS_SMART_LOCK_LABEL_PHONE_LOCKED;
    case SmartLockState::kPhoneAuthenticated:
      return IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER;
  }
  NOTREACHED();
}

bool SmartLockAuthFactorModel::ShouldAnnounceLabel() const {
  return true;
}

int SmartLockAuthFactorModel::GetAccessibleNameId() const {
  // TODO(crbug.com/1233614): Determine whether any state needs to have a
  // different label for a11y.
  return GetLabelId();
}

void SmartLockAuthFactorModel::UpdateIcon(AuthIconView* icon) {
  if (auth_result_.has_value() && !auth_result_.value()) {
    if (has_permanent_error_display_timed_out_) {
      icon->SetIcon(kLockScreenSmartLockDisabledIcon,
                    AuthIconView::Status::kDisabled);
    } else {
      // TODO(crbug.com/1233614): Get actual failure icon once asset is ready.
      icon->SetIcon(kLockScreenSmartCardFailureIcon,
                    AuthIconView::Status::kError);
    }
    icon->StopProgressAnimation();
    return;
  }

  switch (state_) {
    case SmartLockState::kPhoneNotFound:
      icon->SetIcon(kLockScreenSmartLockBluetoothIcon,
                    AuthIconView::Status::kPrimary);
      icon->RunErrorShakeAnimation();
      icon->StopProgressAnimation();
      return;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      icon->SetIcon(kLockScreenSmartLockBluetoothIcon,
                    AuthIconView::Status::kPrimary);
      icon->StopProgressAnimation();
      return;
    case SmartLockState::kConnectingToPhone:
      icon->SetIcon(kLockScreenSmartLockBluetoothIcon,
                    AuthIconView::Status::kPrimary);
      icon->StartProgressAnimation();
      return;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      icon->SetIcon(kLockScreenSmartLockPhoneIcon,
                    AuthIconView::Status::kPrimary);
      icon->StopProgressAnimation();
      return;
    case SmartLockState::kPrimaryUserAbsent:
      [[fallthrough]];
    case SmartLockState::kPhoneNotAuthenticated:
      [[fallthrough]];
    case SmartLockState::kPhoneNotLockable:
      [[fallthrough]];
    case SmartLockState::kBluetoothDisabled:
      icon->SetIcon(kLockScreenSmartLockDisabledIcon,
                    AuthIconView::Status::kDisabled);
      icon->StopProgressAnimation();
      return;
    case SmartLockState::kPhoneAuthenticated:
      // Click to enter -- icon handled by parent view.
      [[fallthrough]];
    case SmartLockState::kDisabled:
      [[fallthrough]];
    case SmartLockState::kInactive:
      // Intentionally blank.
      return;
  }
}

void SmartLockAuthFactorModel::DoHandleTapOrClick() {
  // Do Nothing: Smart Lock does not react to taps on its icon. Clicks on the
  // arrow button are handled in LoginAuthFactorsView.
}

void SmartLockAuthFactorModel::DoHandleErrorTimeout() {
  // Do Nothing: Smart Lock has no temporary errors to restore from, so there is
  // nothing to do.
}

}  // namespace ash
