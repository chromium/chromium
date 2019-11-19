// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"

#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"

namespace virtual_machines {

bool AreVirtualMachinesAllowedByPolicy() {
  bool virtual_machines_allowed;
  if (chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kVirtualMachinesAllowed, &virtual_machines_allowed)) {
    return virtual_machines_allowed;
  }
  // If device policy is not set, allow virtual machines.
  return true;
}

}  // namespace virtual_machines
