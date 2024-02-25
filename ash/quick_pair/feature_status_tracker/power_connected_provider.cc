// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/power_connected_provider.h"

namespace ash {
namespace quick_pair {

PowerConnectedProvider::PowerConnectedProvider() {
  if (PowerStatus::IsInitialized()) {
    power_status_ = PowerStatus::Get();
    power_status_->AddObserver(this);
    SetEnabledAndInvokeCallback(power_status_->IsLinePowerConnected());
    return;
  }
}

PowerConnectedProvider::~PowerConnectedProvider() {
  if (power_status_) {
    power_status_->RemoveObserver(this);
  }
}

void PowerConnectedProvider::OnPowerStatusChanged() {
  SetEnabledAndInvokeCallback(
      /*new_value=*/power_status_ && power_status_->IsLinePowerConnected());
}

}  // namespace quick_pair
}  // namespace ash
