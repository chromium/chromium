// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fake_fingerprint_auth_factor_model.h"

namespace ash {
FakeFingerprintAuthFactorModel::FakeFingerprintAuthFactorModel(
    FingerprintState state)
    : FingerprintAuthFactorModel(state) {}

FakeFingerprintAuthFactorModel::~FakeFingerprintAuthFactorModel() = default;

std::unique_ptr<FingerprintAuthFactorModel>
FakeFingerprintAuthFactorModelFactory::CreateInstance(FingerprintState state) {
  return std::make_unique<FakeFingerprintAuthFactorModel>(state);
}

}  // namespace ash
