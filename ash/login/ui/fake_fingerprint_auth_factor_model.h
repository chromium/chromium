// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FAKE_FINGERPRINT_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_FAKE_FINGERPRINT_AUTH_FACTOR_MODEL_H_

#include "ash/login/ui/fingerprint_auth_factor_model.h"

namespace ash {

class FakeFingerprintAuthFactorModel : public FingerprintAuthFactorModel {
 public:
  explicit FakeFingerprintAuthFactorModel(FingerprintState state);

  FakeFingerprintAuthFactorModel(const FakeFingerprintAuthFactorModel&) =
      delete;
  FakeFingerprintAuthFactorModel& operator=(
      const FakeFingerprintAuthFactorModel&) = delete;

  ~FakeFingerprintAuthFactorModel() override;
};

class FakeFingerprintAuthFactorModelFactory
    : public FingerprintAuthFactorModel::Factory {
 public:
  FakeFingerprintAuthFactorModelFactory() = default;

  std::unique_ptr<FingerprintAuthFactorModel> CreateInstance(
      FingerprintState state) override;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FAKE_FINGERPRINT_AUTH_FACTOR_MODEL_H_
