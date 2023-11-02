// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/oobe_configuration_waiter.h"

namespace ash {

OOBEConfigurationWaiter::OOBEConfigurationWaiter() {}

OOBEConfigurationWaiter::~OOBEConfigurationWaiter() {
  if (callback_) {
    OobeConfiguration::Get()->RemoveObserver(this);
  }
}

void OOBEConfigurationWaiter::OnOobeConfigurationChanged() {
  DCHECK(OobeConfiguration::Get()->CheckCompleted());
  OobeConfiguration::Get()->RemoveObserver(this);
  std::move(callback_).Run();
}

// Wait until configuration is loaded.
bool OOBEConfigurationWaiter::IsConfigurationLoaded(
    base::OnceClosure callback) {
  DCHECK(!callback_);
  if (OobeConfiguration::Get()->CheckCompleted()) {
    return true;
  }
  OobeConfiguration::Get()->AddAndFireObserver(this);
  callback_ = std::move(callback);
  return false;
}

}  // namespace ash
