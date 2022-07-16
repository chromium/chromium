// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fingerprint_auth_factor_model.h"

#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kFingerprintIconSizeDp = 32;
constexpr int kFingerprintFailedAnimationDurationMs = 700;
constexpr int kFingerprintFailedAnimationNumFrames = 45;
constexpr base::TimeDelta kResetToDefaultMessageDelayMs =
    base::Milliseconds(3000);
constexpr int kResetToDefaultIconDelayMs = 1300;

}  // namespace

FingerprintAuthFactorModel::FingerprintAuthFactorModel() = default;

FingerprintAuthFactorModel::~FingerprintAuthFactorModel() = default;

void FingerprintAuthFactorModel::SetFingerprintState(FingerprintState state) {
  if (state_ == state)
    return;

  reset_state_.Stop();
  if (auth_result_.has_value() && !auth_result_.value()) {
    // Clear failed auth attempt to allow retry.
    auth_result_.reset();
  }
  state_ = state;
  NotifyOnStateChanged();
}

void FingerprintAuthFactorModel::NotifyFingerprintAuthResult(bool result) {
  reset_state_.Stop();
  auth_result_ = result;
  NotifyOnStateChanged();

  if (!result) {
    // Clear failed auth attempt after a delay to allow retry. base::Unretained
    // is safe because |reset_state_| is owned by |this|.
    reset_state_.Start(FROM_HERE,
                       base::Milliseconds(kResetToDefaultIconDelayMs),
                       base::BindOnce(&FingerprintAuthFactorModel::OnResetState,
                                      base::Unretained(this)));
  }
}

void FingerprintAuthFactorModel::SetCanUsePin(bool can_use_pin) {
  can_use_pin_ = can_use_pin;
  NotifyOnStateChanged();
}

AuthFactorModel::AuthFactorState
FingerprintAuthFactorModel::GetAuthFactorState() {
  // TODO(crbug.com/1233614): Calculate the correct AuthFactorState based on the
  // current FingerprintState.
  if (!available_)
    return AuthFactorState::kUnavailable;

  if (auth_result_.has_value()) {
    return auth_result_.value() ? AuthFactorState::kAuthenticated
                                : AuthFactorState::kErrorTemporary;
  }

  return state_ == FingerprintState::UNAVAILABLE ? AuthFactorState::kUnavailable
                                                 : AuthFactorState::kReady;
}

AuthFactorType FingerprintAuthFactorModel::GetType() {
  return AuthFactorType::kFingerprint;
}

int FingerprintAuthFactorModel::GetLabelId() {
  if (auth_result_.has_value()) {
    return auth_result_.value() ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS
                                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED;
  }

  switch (state_) {
    case FingerprintState::UNAVAILABLE:
      FALLTHROUGH;
    case FingerprintState::AVAILABLE_DEFAULT:
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_TOUCH_SENSOR;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      if (can_use_pin_)
        return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_PIN_OR_PASSWORD_REQUIRED;
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_PASSWORD_REQUIRED;
  }
  NOTREACHED();
}

bool FingerprintAuthFactorModel::ShouldAnnounceLabel() {
  return state_ == FingerprintState::DISABLED_FROM_ATTEMPTS ||
         state_ == FingerprintState::DISABLED_FROM_TIMEOUT ||
         (auth_result_.has_value() && !auth_result_.value());
}

int FingerprintAuthFactorModel::GetAccessibleNameId() {
  if (state_ == FingerprintState::DISABLED_FROM_ATTEMPTS)
    return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_DISABLED_FROM_ATTEMPTS;

  return GetLabelId();
}

void FingerprintAuthFactorModel::UpdateIcon(AuthIconView* icon) {
  if (auth_result_.has_value() && !auth_result_.value()) {
    icon->SetAnimationDecoder(
        std::make_unique<HorizontalImageSequenceAnimationDecoder>(
            *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
            base::Milliseconds(kFingerprintFailedAnimationDurationMs),
            kFingerprintFailedAnimationNumFrames),
        AnimatedRoundedImageView::Playback::kSingle);
    return;
  }

  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  const SkColor color =
      state_ == FingerprintState::AVAILABLE_DEFAULT ||
              state_ == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING
          ? icon_color
          : AshColorProvider::Get()->GetDisabledColor(icon_color);

  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
      FALLTHROUGH;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      icon->SetIcon(kLockScreenFingerprintIcon);
      break;
    case FingerprintState::UNAVAILABLE:
      FALLTHROUGH;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      icon->SetImage(gfx::CreateVectorIcon(kLockScreenFingerprintDisabledIcon,
                                           kFingerprintIconSizeDp, color));
      break;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      icon->SetAnimationDecoder(
          std::make_unique<HorizontalImageSequenceAnimationDecoder>(
              *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                  IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
              base::Milliseconds(kFingerprintFailedAnimationDurationMs),
              kFingerprintFailedAnimationNumFrames),
          AnimatedRoundedImageView::Playback::kSingle);
      break;
  }
}

void FingerprintAuthFactorModel::OnTapOrClickEvent() {
  if (state_ == FingerprintState::AVAILABLE_DEFAULT ||
      state_ == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING) {
    SetFingerprintState(FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);
    reset_state_.Start(
        FROM_HERE, kResetToDefaultMessageDelayMs,
        base::BindOnce(&FingerprintAuthFactorModel::SetFingerprintState,
                       base::Unretained(this),
                       FingerprintState::AVAILABLE_DEFAULT));
  }
}

void FingerprintAuthFactorModel::OnResetState() {
  if (auth_result_.has_value() && !auth_result_.value()) {
    // Clear failed auth attempt to allow retry.
    auth_result_.reset();
  }
  NotifyOnStateChanged();
}

}  // namespace ash
