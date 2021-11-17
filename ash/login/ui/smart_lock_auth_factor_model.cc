// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/smart_lock_auth_factor_model.h"

#include "ash/login/ui/auth_icon_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kSmartLockIconSizeDp = 32;

}  // namespace

SmartLockAuthFactorModel::SmartLockAuthFactorModel(
    base::RepeatingCallback<void()> arrow_button_tap_callback)
    : arrow_button_tap_callback_(arrow_button_tap_callback) {}

SmartLockAuthFactorModel::~SmartLockAuthFactorModel() = default;

void SmartLockAuthFactorModel::SetEasyUnlockIconState(
    EasyUnlockIconState state) {
  switch (state) {
    case EasyUnlockIconState::NONE:
      FALLTHROUGH;
    case EasyUnlockIconState::HARDLOCKED:
      SetSmartLockState(SmartLockState::kDisabled);
      break;
    case EasyUnlockIconState::LOCKED_WITH_PROXIMITY_HINT:
      SetSmartLockState(SmartLockState::kPhoneFoundLockedAndDistant);
      break;
    case EasyUnlockIconState::LOCKED:
      SetSmartLockState(SmartLockState::kPhoneNotFound);
      break;
    case EasyUnlockIconState::LOCKED_TO_BE_ACTIVATED:
      SetSmartLockState(SmartLockState::kPhoneFoundLockedAndProximate);
      break;
    case EasyUnlockIconState::UNLOCKED:
      SetSmartLockState(SmartLockState::kPhoneAuthenticated);
      break;
    case EasyUnlockIconState::SPINNER:
      SetSmartLockState(SmartLockState::kConnectingToPhone);
      break;
  }
}

void SmartLockAuthFactorModel::SetSmartLockState(SmartLockState state) {
  if (state_ == state)
    return;

  state_ = state;
  NotifyOnStateChanged();
}

void SmartLockAuthFactorModel::NotifySmartLockAuthResult(bool result) {
  auth_result_ = result;
  NotifyOnStateChanged();
}

AuthFactorModel::AuthFactorState
SmartLockAuthFactorModel::GetAuthFactorState() {
  if (auth_result_.has_value()) {
    return auth_result_.value() ? AuthFactorState::kAuthenticated
                                : AuthFactorState::kErrorPermanent;
  }

  switch (state_) {
    case SmartLockState::kDisabled:
      FALLTHROUGH;
    case SmartLockState::kInactive:
      return AuthFactorState::kUnavailable;
    case SmartLockState::kPhoneNotFound:
      FALLTHROUGH;
    case SmartLockState::kConnectingToPhone:
      FALLTHROUGH;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      FALLTHROUGH;
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      return AuthFactorState::kAvailable;
    case SmartLockState::kPhoneAuthenticated:
      return AuthFactorState::kClickRequired;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      return AuthFactorState::kReady;
    case SmartLockState::kPasswordReentryRequired:
      FALLTHROUGH;
    case SmartLockState::kPrimaryUserAbsent:
      FALLTHROUGH;
    case SmartLockState::kPhoneNotAuthenticated:
      FALLTHROUGH;
    case SmartLockState::kBluetoothDisabled:
      FALLTHROUGH;
    case SmartLockState::kPhoneNotLockable:
      return AuthFactorState::kErrorPermanent;
  }
}

AuthFactorType SmartLockAuthFactorModel::GetType() {
  return AuthFactorType::kSmartLock;
}

int SmartLockAuthFactorModel::GetLabelId() {
  if (auth_result_.has_value()) {
    return auth_result_.value() ? IDS_SMART_LOCK_LABEL_PHONE_LOCKED
                                : IDS_AUTH_FACTOR_LABEL_CANNOT_UNLOCK;
  }

  switch (state_) {
    case SmartLockState::kDisabled:
      FALLTHROUGH;
    case SmartLockState::kInactive:
      FALLTHROUGH;
    case SmartLockState::kPasswordReentryRequired:
      FALLTHROUGH;
    case SmartLockState::kPrimaryUserAbsent:
      FALLTHROUGH;
    case SmartLockState::kPhoneNotAuthenticated:
      return IDS_AUTH_FACTOR_LABEL_UNLOCK_PASSWORD;
    case SmartLockState::kBluetoothDisabled:
      return IDS_SMART_LOCK_LABEL_NO_BLUETOOTH;
    case SmartLockState::kPhoneNotLockable:
      return IDS_SMART_LOCK_LABEL_NO_PHONE_LOCK_SCREEN;
    case SmartLockState::kConnectingToPhone:
      return IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      FALLTHROUGH;
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

bool SmartLockAuthFactorModel::ShouldAnnounceLabel() {
  // TODO(crbug.com/1233614): Return 'true' depending on SmartLockState.
  return false;
}

int SmartLockAuthFactorModel::GetAccessibleNameId() {
  // TODO(crbug.com/1233614): Determine whether any state needs to have a
  // different label for a11y.
  return GetLabelId();
}

void SmartLockAuthFactorModel::UpdateIcon(AuthIconView* icon) {
  const SkColor primary_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  const SkColor disabled_color =
      AshColorProvider::Get()->GetDisabledColor(primary_color);

  // TODO(crbug.com/1233614): Either find a system color to match the color in
  // the Fingerprint animation png sequence, or upload new png files with the
  // right color.
  const SkColor alert_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorAlert);

  if (auth_result_.has_value() && !auth_result_.value()) {
    // TODO(crbug.com/1233614): Get actual failure icon once asset is ready.
    icon->SetImage(gfx::CreateVectorIcon(kLockScreenSmartCardFailureIcon,
                                         kSmartLockIconSizeDp, alert_color));
    return;
  }

  switch (state_) {
    case SmartLockState::kPhoneNotFound:
      FALLTHROUGH;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      FALLTHROUGH;
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      FALLTHROUGH;
    case SmartLockState::kConnectingToPhone:
      icon->SetImage(gfx::CreateVectorIcon(kLockScreenSmartLockBluetoothIcon,
                                           kSmartLockIconSizeDp,
                                           primary_color));
      return;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      icon->SetImage(gfx::CreateVectorIcon(
          kLockScreenSmartLockPhoneIcon, kSmartLockIconSizeDp, primary_color));
      return;
    case SmartLockState::kPrimaryUserAbsent:
      FALLTHROUGH;
    case SmartLockState::kPhoneNotAuthenticated:
      FALLTHROUGH;
    case SmartLockState::kPasswordReentryRequired:
      FALLTHROUGH;
    case SmartLockState::kPhoneNotLockable:
      FALLTHROUGH;
    case SmartLockState::kBluetoothDisabled:
      icon->SetImage(gfx::CreateVectorIcon(kLockScreenSmartLockDisabledIcon,
                                           kSmartLockIconSizeDp,
                                           disabled_color));
      return;
    case SmartLockState::kPhoneAuthenticated:
      // Click to enter -- icon handled by parent view.
      FALLTHROUGH;
    case SmartLockState::kDisabled:
      FALLTHROUGH;
    case SmartLockState::kInactive:
      // Intentionally blank.
      return;
  }
}

void SmartLockAuthFactorModel::OnTapOrClickEvent() {}

void SmartLockAuthFactorModel::OnArrowButtonTapOrClickEvent() {
  if (state_ == SmartLockState::kPhoneAuthenticated) {
    arrow_button_tap_callback_.Run();
  }
}

void SmartLockAuthFactorModel::OnErrorTimeout() {}

}  // namespace ash
