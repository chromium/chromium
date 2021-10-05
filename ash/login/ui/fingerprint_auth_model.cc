// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fingerprint_auth_model.h"

#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kFingerprintIconSizeDp = 28;
constexpr int kFingerprintFailedAnimationDurationMs = 700;
constexpr int kFingerprintFailedAnimationNumFrames = 45;
constexpr base::TimeDelta kResetToDefaultMessageDelayMs =
    base::Milliseconds(3000);
constexpr int kResetToDefaultIconDelayMs = 1300;

}  // namespace

FingerprintAuthModel::FingerprintAuthModel() = default;

FingerprintAuthModel::~FingerprintAuthModel() = default;

void FingerprintAuthModel::SetFingerprintState(FingerprintState state) {
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

void FingerprintAuthModel::NotifyFingerprintAuthResult(bool result) {
  reset_state_.Stop();
  auth_result_ = result;
  NotifyOnStateChanged();

  if (!result) {
    // Clear failed auth attempt after a delay to allow retry. base::Unretained
    // is safe because |reset_state_| is owned by |this|.
    reset_state_.Start(FROM_HERE,
                       base::Milliseconds(kResetToDefaultIconDelayMs),
                       base::BindOnce(&FingerprintAuthModel::OnResetState,
                                      base::Unretained(this)));
  }
}

void FingerprintAuthModel::SetCanUsePin(bool can_use_pin) {
  can_use_pin_ = can_use_pin;
  NotifyOnStateChanged();
}

AuthFactorModel::AuthFactorState FingerprintAuthModel::GetAuthFactorState() {
  // TODO(crbug.com/1233614): Calculate the correct AuthFactorState based on the
  // current FingerprintState.
  if (!visible_)
    return AuthFactorState::kUnavailable;

  return state_ == FingerprintState::UNAVAILABLE ? AuthFactorState::kUnavailable
                                                 : AuthFactorState::kReady;
}

AuthFactorType FingerprintAuthModel::GetType() {
  return AuthFactorType::kFingerprint;
}

std::u16string FingerprintAuthModel::GetLabel() {
  auto get_displayed_id = [&]() {
    if (auth_result_.has_value()) {
      return auth_result_.value()
                 ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS
                 : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED;
    }

    switch (state_) {
      case FingerprintState::UNAVAILABLE:
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
  };

  return l10n_util::GetStringUTF16(get_displayed_id());
}

bool FingerprintAuthModel::ShouldAnnounceLabel() {
  return state_ == FingerprintState::DISABLED_FROM_ATTEMPTS ||
         state_ == FingerprintState::DISABLED_FROM_TIMEOUT ||
         (auth_result_.has_value() && !auth_result_.value());
}

std::u16string FingerprintAuthModel::GetAccessibleName() {
  if (state_ == FingerprintState::DISABLED_FROM_ATTEMPTS)
    return l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_DISABLED_FROM_ATTEMPTS);
  return GetLabel();
}

void FingerprintAuthModel::UpdateIcon(AuthIconView* icon_view) {
  if (auth_result_.has_value()) {
    if (auth_result_.value()) {
      // We do not need to treat the light/dark mode for this use-case since
      // this hint is shown for a short time interval.
      icon_view->SetImage(gfx::CreateVectorIcon(
          kLockScreenFingerprintSuccessIcon, kFingerprintIconSizeDp,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPositive)));
    } else {
      icon_view->SetAnimationDecoder(
          std::make_unique<HorizontalImageSequenceAnimationDecoder>(
              *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                  IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
              base::Milliseconds(kFingerprintFailedAnimationDurationMs),
              kFingerprintFailedAnimationNumFrames),
          AnimatedRoundedImageView::Playback::kSingle);
    }
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
    case FingerprintState::UNAVAILABLE:
    case FingerprintState::AVAILABLE_DEFAULT:
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      icon_view->SetImage(gfx::CreateVectorIcon(kLockScreenFingerprintIcon,
                                                kFingerprintIconSizeDp, color));
      break;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      icon_view->SetAnimationDecoder(
          std::make_unique<HorizontalImageSequenceAnimationDecoder>(
              *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                  IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
              base::Milliseconds(kFingerprintFailedAnimationDurationMs),
              kFingerprintFailedAnimationNumFrames),
          AnimatedRoundedImageView::Playback::kSingle);
      break;
  }
}

void FingerprintAuthModel::OnTapEvent() {
  if (state_ == FingerprintState::AVAILABLE_DEFAULT ||
      state_ == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING) {
    SetFingerprintState(FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);
    reset_state_.Start(
        FROM_HERE, kResetToDefaultMessageDelayMs,
        base::BindOnce(&FingerprintAuthModel::SetFingerprintState,
                       base::Unretained(this),
                       FingerprintState::AVAILABLE_DEFAULT));
  }
}

void FingerprintAuthModel::OnResetState() {
  if (auth_result_.has_value() && !auth_result_.value()) {
    // Clear failed auth attempt to allow retry.
    auth_result_.reset();
  }
  NotifyOnStateChanged();
}

void FingerprintAuthModel::SetVisible(bool visible) {
  visible_ = visible;
}

}  // namespace ash
