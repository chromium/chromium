// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FINGERPRINT_AUTH_MODEL_H_
#define ASH_LOGIN_UI_FINGERPRINT_AUTH_MODEL_H_

#include <string>

#include "ash/login/ui/auth_factor_model.h"

namespace ash {

class AuthIconView;

// Implements the logic necessary to show Fingerprint as an auth factor on the
// lock screen.
// TODO(crbug.com/1233614): Finish migrating FingerprintView to this class.
class FingerprintAuthModel : public AuthFactorModel {
 public:
  FingerprintAuthModel();
  FingerprintAuthModel(FingerprintAuthModel&) = delete;
  FingerprintAuthModel& operator=(FingerprintAuthModel&) = delete;
  ~FingerprintAuthModel() override;

  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() override;
  AuthFactorType GetType() override;
  std::u16string GetLabel() override;
  bool ShouldAnnounceLabel() override;
  void UpdateIcon(AuthIconView* icon_view) override;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FINGERPRINT_AUTH_MODEL_H_
