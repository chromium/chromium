// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <vector>

#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

AshAcceleratorConfiguration::AshAcceleratorConfiguration()
    : AcceleratorConfiguration(ash::mojom::AcceleratorSource::kAsh) {}
AshAcceleratorConfiguration::~AshAcceleratorConfiguration() = default;

// TODO(jimmyxgong): Implement all functions below as these are only stubs.
const std::vector<AcceleratorInfo>&
AshAcceleratorConfiguration::GetConfigForAction(AcceleratorAction actionId) {
  return accelerator_infos_;
}

bool AshAcceleratorConfiguration::IsMutable() const {
  return false;
}

AcceleratorConfigResult AshAcceleratorConfiguration::AddUserAccelerator(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RemoveAccelerator(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::ReplaceAccelerator(
    AcceleratorAction action,
    const ui::Accelerator& old_acc,
    const ui::Accelerator& new_acc) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RestoreDefault(
    AcceleratorAction action) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RestoreAllDefaults() {
  return AcceleratorConfigResult::kActionLocked;
}

}  // namespace ash
