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
  SmartLockAuthFactorModel();
  SmartLockAuthFactorModel(SmartLockAuthFactorModel&) = delete;
  SmartLockAuthFactorModel& operator=(SmartLockAuthFactorModel&) = delete;
  ~SmartLockAuthFactorModel() override;

  // TODO(crbug.com/1233614): Remove this once SmartLockState is passed in
  // instead of EasyUnlockIconState.
  void SetEasyUnlockIconState(EasyUnlockIconState state);

  void SetSmartLockState(SmartLockState state);
  void NotifySmartLockAuthResult(bool result);

 private:
  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() override;
  AuthFactorType GetType() override;
  int GetLabelId() override;
  bool ShouldAnnounceLabel() override;
  int GetAccessibleNameId() override;
  void UpdateIcon(AuthIconView* icon) override;
  void OnTapOrClickEvent() override;

  SmartLockState state_ = SmartLockState::kInactive;
  absl::optional<bool> auth_result_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_SMART_LOCK_AUTH_FACTOR_MODEL_H_
