// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FINGERPRINT_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_FINGERPRINT_AUTH_FACTOR_MODEL_H_

#include "ash/login/ui/auth_factor_model.h"
#include "ash/public/cpp/login_types.h"

namespace ash {

class AuthIconView;

// Implements the logic necessary to show Fingerprint as an auth factor on the
// lock screen.
class FingerprintAuthFactorModel : public AuthFactorModel {
 public:
  FingerprintAuthFactorModel();
  FingerprintAuthFactorModel(FingerprintAuthFactorModel&) = delete;
  FingerprintAuthFactorModel& operator=(FingerprintAuthFactorModel&) = delete;
  ~FingerprintAuthFactorModel() override;

  void SetFingerprintState(FingerprintState state);
  void NotifyFingerprintAuthResult(bool result);

  // If |available| is false, forces |GetAuthFactorState()| to return
  // |kUnavailable|, otherwise has no effect. Used to hide Fingerprint auth
  // independently of |state_|.
  void set_available(bool available) { available_ = available; }

 private:
  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() const override;
  AuthFactorType GetType() const override;
  int GetLabelId() const override;
  bool ShouldAnnounceLabel() const override;
  int GetAccessibleNameId() const override;
  void DoHandleTapOrClick() override;
  void DoHandleErrorTimeout() override;
  void UpdateIcon(AuthIconView* icon) override;

  FingerprintState state_ = FingerprintState::AVAILABLE_DEFAULT;
  absl::optional<bool> auth_result_;

  bool available_ = true;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FINGERPRINT_AUTH_FACTOR_MODEL_H_
