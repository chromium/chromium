// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <vector>

#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"

namespace {

void AppendAcceleratorData(std::vector<ash::AcceleratorData>& data,
                           const ash::AcceleratorData accelerators[],
                           size_t accelerators_length) {
  data.reserve(data.size() + accelerators_length);
  for (size_t i = 0; i < accelerators_length; ++i) {
    data.push_back(accelerators[i]);
  }
}

}  // namespace

namespace ash {

AshAcceleratorConfiguration::AshAcceleratorConfiguration()
    : AcceleratorConfiguration(ash::mojom::AcceleratorSource::kAsh) {}
AshAcceleratorConfiguration::~AshAcceleratorConfiguration() = default;

// TODO(jimmyxgong): Implement all functions below as these are only stubs.

const std::vector<mojom::AcceleratorLayoutInfoPtr>&
AshAcceleratorConfiguration::GetAcceleratorLayoutInfos() {
  return layout_infos_;
}

const std::vector<AcceleratorInfo>&
AshAcceleratorConfiguration::GetConfigForAction(AcceleratorActionId action_id) {
  DCHECK(base::Contains(id_to_accelerator_infos_, action_id));

  return id_to_accelerator_infos_[action_id];
}

bool AshAcceleratorConfiguration::IsMutable() const {
  return false;
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
  AppendAcceleratorData(accelerators, kAcceleratorData, kAcceleratorDataLength);

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    AppendAcceleratorData(accelerators, kEnableWithPositionalAcceleratorsData,
                          kEnableWithPositionalAcceleratorsDataLength);
    if (ash::features::IsImprovedDesksKeyboardShortcutsEnabled()) {
      AppendAcceleratorData(
          accelerators,
          kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData,
          kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength);
    }
  } else if (::features::IsNewShortcutMappingEnabled()) {
    AppendAcceleratorData(accelerators, kEnableWithNewMappingAcceleratorData,
                          kEnableWithNewMappingAcceleratorDataLength);
  } else {
    AppendAcceleratorData(accelerators, kDisableWithNewMappingAcceleratorData,
                          kDisableWithNewMappingAcceleratorDataLength);
  }

  // Debug accelerators.
  if (debug::DebugAcceleratorsEnabled()) {
    AppendAcceleratorData(accelerators, kDebugAcceleratorData,
                          kDebugAcceleratorDataLength);
  }

  // Developer accelerators.
  if (debug::DeveloperAcceleratorsEnabled()) {
    AppendAcceleratorData(accelerators, kDeveloperAcceleratorData,
                          kDeveloperAcceleratorDataLength);
  }

  Initialize(accelerators);
  InitializeDeprecatedAccelerators();
}

void AshAcceleratorConfiguration::Initialize(
    base::span<const AcceleratorData> accelerators) {
  accelerator_infos_.clear();
  id_to_accelerator_infos_.clear();
  accelerator_to_id_.Clear();

  AddAccelerators(accelerators, mojom::AcceleratorType::kDefault);
}

void AshAcceleratorConfiguration::InitializeDeprecatedAccelerators() {
  base::span<const DeprecatedAcceleratorData> deprecated_accelerator_data(
      kDeprecatedAcceleratorsData, kDeprecatedAcceleratorsDataLength);
  base::span<const AcceleratorData> deprecated_accelerators(
      kDeprecatedAccelerators, kDeprecatedAcceleratorsLength);

  InitializeDeprecatedAccelerators(std::move(deprecated_accelerator_data),
                                   std::move(deprecated_accelerators));
}

void AshAcceleratorConfiguration::InitializeDeprecatedAccelerators(
    base::span<const DeprecatedAcceleratorData> deprecated_data,
    base::span<const AcceleratorData> deprecated_accelerators) {
  for (const auto& deprecated_data : deprecated_data) {
    const DeprecatedAcceleratorData* data = &deprecated_data;
    actions_with_deprecations_[data->action] = data;
  }

  AddAccelerators(deprecated_accelerators, mojom::AcceleratorType::kDeprecated);
}

void AshAcceleratorConfiguration::AddAccelerators(
    base::span<const AcceleratorData> accelerators,
    mojom::AcceleratorType type) {
  for (const auto& data : accelerators) {
    ui::Accelerator accelerator(data.keycode, data.modifiers);
    accelerator.set_key_state(data.trigger_on_press
                                  ? ui::Accelerator::KeyState::PRESSED
                                  : ui::Accelerator::KeyState::RELEASED);
    // TODO(jimmyxgong): Ash accelerators should not be locked when
    // customization is allowed.
    AcceleratorInfo info(type, accelerator, /**locked=*/true);
    accelerator_to_id_.InsertNew(std::make_pair(accelerator, data.action));
    id_to_accelerator_infos_[static_cast<uint32_t>(data.action)].push_back(
        info);
    accelerator_infos_.push_back(info);
    AddLayoutInfo(data);
  }
  UpdateAccelerators(id_to_accelerator_infos_);
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

bool AshAcceleratorConfiguration::IsDeprecated(
    const ui::Accelerator& accelerator) {
  const auto* action_id = FindAcceleratorAction(accelerator);
  // Not a registered accelerator, return false.
  if (!action_id) {
    return false;
  }
  DCHECK(base::Contains(id_to_accelerator_infos_, *action_id));

  const std::vector<AcceleratorInfo> infos =
      id_to_accelerator_infos_.at(*action_id);
  for (auto const& info : infos) {
    if (info.type == mojom::AcceleratorType::kDeprecated &&
        info.accelerator == accelerator) {
      return true;
    }
  }
  return false;
}

void AshAcceleratorConfiguration::AddLayoutInfo(const AcceleratorData& data) {
  // TODO(jimmyxgong): This a basic stub implementation, replace with real
  // implementation.
  mojom::AcceleratorLayoutInfoPtr layout_info =
      mojom::AcceleratorLayoutInfo::New();
  layout_info->category = mojom::AcceleratorCategory::kSystem;
  layout_info->sub_category = mojom::AcceleratorSubcategory::kGeneral;
  // TODO(jimmyxgong): Create a mapping between action_id and description.
  layout_info->description = u"Stub description";
  layout_info->style = mojom::AcceleratorLayoutStyle::kDefault;
  layout_info->source = mojom::AcceleratorSource::kAsh;
  layout_info->action = static_cast<uint32_t>(data.action);

  layout_infos_.push_back(std::move(layout_info));
}

}  // namespace ash
