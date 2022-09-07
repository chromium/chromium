// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fake_smart_lock_auth_factor_model.h"

namespace ash {

FakeSmartLockAuthFactorModel::FakeSmartLockAuthFactorModel(
    SmartLockState initial_state,
    base::RepeatingCallback<void()> arrow_button_tap_callback)
    : SmartLockAuthFactorModel(initial_state, arrow_button_tap_callback) {}

FakeSmartLockAuthFactorModel::~FakeSmartLockAuthFactorModel() = default;

SmartLockState FakeSmartLockAuthFactorModel::GetSmartLockState() {
  return state_;
}

std::unique_ptr<SmartLockAuthFactorModel>
FakeSmartLockAuthFactorModelFactory::CreateInstance(
    SmartLockState initial_state,
    base::RepeatingCallback<void()> arrow_button_tap_callback) {
  auto fake_smart_lock_auth_factor_model =
      std::make_unique<FakeSmartLockAuthFactorModel>(initial_state,
                                                     arrow_button_tap_callback);
  last_created_model_ = fake_smart_lock_auth_factor_model.get();
  return fake_smart_lock_auth_factor_model;
}

FakeSmartLockAuthFactorModel*
FakeSmartLockAuthFactorModelFactory::GetLastCreatedModel() {
  return last_created_model_;
}

}  // namespace ash
