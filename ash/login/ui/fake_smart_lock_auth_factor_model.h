// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FAKE_SMART_LOCK_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_FAKE_SMART_LOCK_AUTH_FACTOR_MODEL_H_

#include "ash/login/ui/smart_lock_auth_factor_model.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class FakeSmartLockAuthFactorModel : public SmartLockAuthFactorModel {
 public:
  FakeSmartLockAuthFactorModel(
      SmartLockState initial_state,
      base::RepeatingCallback<void()> arrow_button_tap_callback);

  FakeSmartLockAuthFactorModel(const FakeSmartLockAuthFactorModel&) = delete;
  FakeSmartLockAuthFactorModel& operator=(const FakeSmartLockAuthFactorModel&) =
      delete;

  ~FakeSmartLockAuthFactorModel() override;

  SmartLockState GetSmartLockState();
};

class FakeSmartLockAuthFactorModelFactory
    : public SmartLockAuthFactorModel::Factory {
 public:
  FakeSmartLockAuthFactorModelFactory() = default;

  std::unique_ptr<SmartLockAuthFactorModel> CreateInstance(
      SmartLockState initial_state,
      base::RepeatingCallback<void()> arrow_button_tap_callback) override;

  FakeSmartLockAuthFactorModel* GetLastCreatedModel();

 private:
  raw_ptr<FakeSmartLockAuthFactorModel, DanglingUntriaged> last_created_model_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FAKE_SMART_LOCK_AUTH_FACTOR_MODEL_H_
