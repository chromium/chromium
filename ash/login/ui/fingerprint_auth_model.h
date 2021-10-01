// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FINGERPRINT_AUTH_MODEL_H_
#define ASH_LOGIN_UI_FINGERPRINT_AUTH_MODEL_H_

#include <string>

#include "ash/login/ui/auth_factor_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/timer/timer.h"

namespace ash {

class AuthIconView;

// Implements the logic necessary to show Fingerprint as an auth factor on the
// lock screen.
class FingerprintAuthModel : public AuthFactorModel {
 public:
  FingerprintAuthModel();
  FingerprintAuthModel(FingerprintAuthModel&) = delete;
  FingerprintAuthModel& operator=(FingerprintAuthModel&) = delete;
  ~FingerprintAuthModel() override;

  void SetFingerprintState(FingerprintState state);
  void NotifyFingerprintAuthResult(bool result);
  void SetCanUsePin(bool can_use_pin);
  void SetVisible(bool visible);

  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() override;
  AuthFactorType GetType() override;
  std::u16string GetLabel() override;
  bool ShouldAnnounceLabel() override;
  std::u16string GetAccessibleName() override;
  void UpdateIcon(AuthIconView* icon_view) override;
  void OnTapEvent() override;

 private:
  void OnResetState();

  FingerprintState state_ = FingerprintState::AVAILABLE_DEFAULT;
  absl::optional<bool> auth_result_;

  base::OneShotTimer reset_state_;

  // Affects DISABLED_FROM_TIMEOUT message.
  bool can_use_pin_ = false;
  bool visible_ = true;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FINGERPRINT_AUTH_MODEL_H_
