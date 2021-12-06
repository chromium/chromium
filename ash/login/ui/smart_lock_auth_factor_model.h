// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_SMART_LOCK_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_SMART_LOCK_AUTH_FACTOR_MODEL_H_

#include "ash/ash_export.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/smartlock_state.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class AuthIconView;

// Implements the logic necessary to show Smart Lock as an auth factor on the
// lock screen.
class ASH_EXPORT SmartLockAuthFactorModel : public AuthFactorModel {
 public:
  SmartLockAuthFactorModel(
      base::RepeatingCallback<void()> arrow_button_tap_callback);
  SmartLockAuthFactorModel(SmartLockAuthFactorModel&) = delete;
  SmartLockAuthFactorModel& operator=(SmartLockAuthFactorModel&) = delete;
  ~SmartLockAuthFactorModel() override;

  // AuthFactorModel:
  void OnArrowButtonTapOrClickEvent() override;

  // TODO(crbug.com/1233614): Remove this once SmartLockState is passed in
  // instead of EasyUnlockIconState.
  void SetEasyUnlockIconState(EasyUnlockIconState state);

  void SetSmartLockState(SmartLockState state);
  void NotifySmartLockAuthResult(bool result);

 private:
  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() const override;
  AuthFactorType GetType() const override;
  int GetLabelId() const override;
  bool ShouldAnnounceLabel() const override;
  int GetAccessibleNameId() const override;
  void UpdateIcon(AuthIconView* icon) override;
  void DoHandleTapOrClick() override;
  void DoHandleErrorTimeout() override;

  base::RepeatingCallback<void()> arrow_button_tap_callback_;
  SmartLockState state_ = SmartLockState::kInactive;
  absl::optional<bool> auth_result_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_SMART_LOCK_AUTH_FACTOR_MODEL_H_
