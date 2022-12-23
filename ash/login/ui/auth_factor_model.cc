// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"

namespace ash {

// static
bool AuthFactorModel::can_use_pin_ = false;

AuthFactorModel::AuthFactorModel() = default;

AuthFactorModel::~AuthFactorModel() = default;

void AuthFactorModel::Init(AuthIconView* icon,
                           base::RepeatingClosure update_state_callback) {
  DCHECK(!icon_) << "Init should only be called once.";
  icon_ = icon;
  update_state_callback_ = update_state_callback;
}

void AuthFactorModel::SetVisible(bool visible) {
  DCHECK(icon_);
  icon_->SetVisible(visible);
}

void AuthFactorModel::OnThemeChanged() {
  if (icon_) {
    UpdateIcon(icon_);
  }
}

void AuthFactorModel::HandleTapOrClick() {
  // If an auth factor icon is clicked while the auth factor has a permanent
  // error, then show the error again by marking it as not timed out.
  if (GetAuthFactorState() == AuthFactorState::kErrorPermanent) {
    has_permanent_error_display_timed_out_ = false;
  }

  DoHandleTapOrClick();

  // Call `RefreshUI` here in case |has_permanent_error_display_timed_out_|
  // changed. It's called regardless of whether or not there was an actual
  // change so that `DoHandleTapOrClick` can avoid calling `RefreshUI`, which
  // could result in multiple calls.
  RefreshUI();
}

void AuthFactorModel::HandleErrorTimeout() {
  if (GetAuthFactorState() == AuthFactorState::kErrorPermanent) {
    has_permanent_error_display_timed_out_ = true;
  }

  DoHandleErrorTimeout();

  // Call `RefreshUI` here in case |has_permanent_error_display_timed_out_|
  // changed. It's called regardless of whether or not there was an actual
  // change so that `DoHandleErrorTimeout` can avoid calling `RefreshUI`, which
  // could result in multiple calls.
  RefreshUI();
}

void AuthFactorModel::RefreshUI() {
  DCHECK(icon_);
  if (update_state_callback_) {
    update_state_callback_.Run();
  }
  UpdateIcon(icon_);
}

void AuthFactorModel::set_can_use_pin(bool can_use_pin) {
  can_use_pin_ = can_use_pin;
}

bool AuthFactorModel::can_use_pin() {
  return can_use_pin_;
}

void AuthFactorModel::OnArrowButtonTapOrClickEvent() {}

}  // namespace ash
