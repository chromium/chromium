// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fake_smart_lock_auth_factor_model.h"

namespace ash {

FakeSmartLockAuthFactorModel::FakeSmartLockAuthFactorModel(
    base::RepeatingCallback<void()> arrow_button_tap_callback)
    : SmartLockAuthFactorModel(arrow_button_tap_callback) {}

FakeSmartLockAuthFactorModel::~FakeSmartLockAuthFactorModel() = default;

std::unique_ptr<SmartLockAuthFactorModel>
FakeSmartLockAuthFactorModelFactory::CreateInstance(
    base::RepeatingCallback<void()> arrow_button_tap_callback) {
  return std::make_unique<FakeSmartLockAuthFactorModel>(
      arrow_button_tap_callback);
}

}  // namespace ash
