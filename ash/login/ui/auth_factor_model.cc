// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_factor_model.h"

namespace ash {

int operator|(int types, AuthFactorType type) {
  return types | static_cast<int>(type);
}

int operator|(AuthFactorType type1, AuthFactorType type2) {
  return static_cast<int>(type1) | static_cast<int>(type2);
}

AuthFactorModel::AuthFactorModel() = default;

AuthFactorModel::~AuthFactorModel() = default;

void AuthFactorModel::SetOnStateChangedCallback(
    base::RepeatingClosure on_state_changed) {
  on_state_changed_callback_ = on_state_changed;
}

void AuthFactorModel::NotifyOnStateChanged() {
  if (on_state_changed_callback_)
    on_state_changed_callback_.Run();
}

}  // namespace ash
