// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <vector>

#include "ash/accelerators/accelerator_layout_table.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace {

void AppendAcceleratorData(
    std::vector<ash::AcceleratorData>& data,
    base::span<const ash::AcceleratorData> accelerators) {
  data.reserve(data.size() + accelerators.size());
  for (const auto& accelerator : accelerators) {
    data.push_back(accelerator);
  }
}

}  // namespace

namespace ash {

AshAcceleratorConfiguration::AshAcceleratorConfiguration()
    : AcceleratorConfiguration(ash::mojom::AcceleratorSource::kAsh) {}
AshAcceleratorConfiguration::~AshAcceleratorConfiguration() = default;

// TODO(jimmyxgong): Implement all functions below as these are only stubs.

const std::vector<ui::Accelerator>&
AshAcceleratorConfiguration::GetAcceleratorsForAction(
    AcceleratorActionId action_id) {
  const auto accelerator_iter = id_to_accelerators_.find(action_id);
  DCHECK(accelerator_iter != id_to_accelerators_.end());

  return accelerator_iter->second;
}

bool AshAcceleratorConfiguration::IsMutable() const {
  // TODO(longbowei): Implement this function as this is only stub for now.
  return false;
}

bool AshAcceleratorConfiguration::IsDeprecated(
    const ui::Accelerator& accelerator) const {
  return deprecated_accelerators_.contains(accelerator);
}

AcceleratorConfigResult AshAcceleratorConfiguration::AddUserAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RemoveAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::ReplaceAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& old_acc,
    const ui::Accelerator& new_acc) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RestoreDefault(
    AcceleratorActionId action_id) {
  return AcceleratorConfigResult::kActionLocked;
}

AcceleratorConfigResult AshAcceleratorConfiguration::RestoreAllDefaults() {
  return AcceleratorConfigResult::kActionLocked;
}

void AshAcceleratorConfiguration::Initialize() {
  std::vector<AcceleratorData> accelerators;
  AppendAcceleratorData(
      accelerators, base::make_span(kAcceleratorData, kAcceleratorDataLength));

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(kEnableWithPositionalAcceleratorsData,
                        kEnableWithPositionalAcceleratorsDataLength));
    AppendAcceleratorData(
        accelerators,
        base::make_span(
            kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData,
            kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength));
  } else if (::features::IsNewShortcutMappingEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(kEnableWithNewMappingAcceleratorData,
                        kEnableWithNewMappingAcceleratorDataLength));
  } else {
    AppendAcceleratorData(
        accelerators,
        base::make_span(kDisableWithNewMappingAcceleratorData,
                        kDisableWithNewMappingAcceleratorDataLength));
  }
  if (ash::features::IsSameAppWindowCycleEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(kEnableWithSameAppWindowCycleAcceleratorData,
                        kEnableWithSameAppWindowCycleAcceleratorDataLength));
  }

  // Debug accelerators.
  if (debug::DebugAcceleratorsEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(kDebugAcceleratorData, kDebugAcceleratorDataLength));
  }

  // Developer accelerators.
  if (debug::DeveloperAcceleratorsEnabled()) {
    AppendAcceleratorData(accelerators,
                          base::make_span(kDeveloperAcceleratorData,
                                          kDeveloperAcceleratorDataLength));
  }

  Initialize(accelerators);
  InitializeDeprecatedAccelerators();
}

void AshAcceleratorConfiguration::Initialize(
    base::span<const AcceleratorData> accelerators) {
  accelerators_.clear();
  deprecated_accelerators_.clear();
  id_to_accelerators_.clear();
  accelerator_to_id_.Clear();

  AddAccelerators(accelerators);
}

void AshAcceleratorConfiguration::InitializeDeprecatedAccelerators() {
  base::span<const DeprecatedAcceleratorData> deprecated_accelerator_data(
      kDeprecatedAcceleratorsData, kDeprecatedAcceleratorsDataLength);
  base::span<const AcceleratorData> deprecated_accelerators(
      kDeprecatedAccelerators, kDeprecatedAcceleratorsLength);

  InitializeDeprecatedAccelerators(std::move(deprecated_accelerator_data),
                                   std::move(deprecated_accelerators));
}

// This function must only be called after Initialize().
void AshAcceleratorConfiguration::InitializeDeprecatedAccelerators(
    base::span<const DeprecatedAcceleratorData> deprecated_data,
    base::span<const AcceleratorData> deprecated_accelerators) {
  for (const auto& data : deprecated_data) {
    actions_with_deprecations_[data.action] = &data;
  }

  for (const auto& data : deprecated_accelerators) {
    deprecated_accelerators_.emplace(data.keycode, data.modifiers);
  }

  AddAccelerators(deprecated_accelerators);
}

void AshAcceleratorConfiguration::AddAccelerators(
    base::span<const AcceleratorData> accelerators) {
  accelerators_.reserve(accelerators_.size() + accelerators.size());
  for (const auto& data : accelerators) {
    ui::Accelerator accelerator(data.keycode, data.modifiers);
    accelerator.set_key_state(data.trigger_on_press
                                  ? ui::Accelerator::KeyState::PRESSED
                                  : ui::Accelerator::KeyState::RELEASED);
    accelerator_to_id_.InsertNew(std::make_pair(accelerator, data.action));
    id_to_accelerators_[static_cast<uint32_t>(data.action)].push_back(
        accelerator);
    accelerators_.push_back(accelerator);
  }
  UpdateAccelerators(id_to_accelerators_);
}

const DeprecatedAcceleratorData*
AshAcceleratorConfiguration::GetDeprecatedAcceleratorData(
    AcceleratorActionId action) {
  auto it = actions_with_deprecations_.find(action);
  if (it == actions_with_deprecations_.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace ash
