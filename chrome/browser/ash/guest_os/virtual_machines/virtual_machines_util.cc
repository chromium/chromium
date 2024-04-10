// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"

#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace virtual_machines {

bool AreVirtualMachinesAllowedByPolicy() {
  bool virtual_machines_allowed;
  if (ash::CrosSettings::Get()->GetBoolean(ash::kVirtualMachinesAllowed,
                                           &virtual_machines_allowed)) {
    return virtual_machines_allowed;
  }
  // If device policy is not set, allow virtual machines.
  return true;
}

}  // namespace virtual_machines
