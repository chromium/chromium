// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <vector>

#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/wm/features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace {

using AcceleratorActionMap = ui::AcceleratorMap<ash::AcceleratorAction>;

void AppendAcceleratorData(
    std::vector<ash::AcceleratorData>& data,
    base::span<const ash::AcceleratorData> accelerators) {
  data.reserve(data.size() + accelerators.size());
  for (const auto& accelerator : accelerators) {
    data.push_back(accelerator);
  }
}

void SetLookupMaps(base::span<const ash::AcceleratorData> accelerators,
                   ash::ActionIdToAcceleratorsMap& id_to_accelerator,
                   AcceleratorActionMap& accelerator_to_id) {
  for (const auto& acceleratorData : accelerators) {
    ui::Accelerator accelerator(acceleratorData.keycode,
                                acceleratorData.modifiers);
    accelerator.set_key_state(acceleratorData.trigger_on_press
                                  ? ui::Accelerator::KeyState::PRESSED
                                  : ui::Accelerator::KeyState::RELEASED);
    accelerator_to_id.InsertNew(
        std::make_pair(accelerator, acceleratorData.action));
    id_to_accelerator[static_cast<uint32_t>(acceleratorData.action)].push_back(
        accelerator);
  }
}

std::vector<ash::AcceleratorData> GetDefaultAccelerators() {
  std::vector<ash::AcceleratorData> accelerators;
  AppendAcceleratorData(
      accelerators,
      base::make_span(ash::kAcceleratorData, ash::kAcceleratorDataLength));

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kEnableWithPositionalAcceleratorsData,
                        ash::kEnableWithPositionalAcceleratorsDataLength));
    AppendAcceleratorData(
        accelerators,
        base::make_span(
            ash::kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData,
            ash::
                kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength));
  } else if (::features::IsNewShortcutMappingEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kEnableWithNewMappingAcceleratorData,
                        ash::kEnableWithNewMappingAcceleratorDataLength));
  } else {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kDisableWithNewMappingAcceleratorData,
                        ash::kDisableWithNewMappingAcceleratorDataLength));
  }
  if (ash::features::IsSameAppWindowCycleEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(
            ash::kEnableWithSameAppWindowCycleAcceleratorData,
            ash::kEnableWithSameAppWindowCycleAcceleratorDataLength));
  }
  if (chromeos::wm::features::IsWindowLayoutMenuEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kEnableWithFloatWindowAcceleratorData,
                        ash::kEnableWithFloatWindowAcceleratorDataLength));
  }
  if (ash::features::IsGameDashboardEnabled()) {
    AppendAcceleratorData(
        accelerators,
        base::make_span(ash::kToggleGameDashboardAcceleratorData,
                        ash::kToggleGameDashboardAcceleratorDataLength));
  }

  // Debug accelerators.
  if (ash::debug::DebugAcceleratorsEnabled()) {
    AppendAcceleratorData(accelerators,
                          base::make_span(ash::kDebugAcceleratorData,
                                          ash::kDebugAcceleratorDataLength));
  }

  // Developer accelerators.
  if (ash::debug::DeveloperAcceleratorsEnabled()) {
    AppendAcceleratorData(
        accelerators, base::make_span(ash::kDeveloperAcceleratorData,
                                      ash::kDeveloperAcceleratorDataLength));
  }
  return accelerators;
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
  DCHECK(::features::IsShortcutCustomizationEnabled());
  AcceleratorConfigResult result = DoRemoveAccelerator(action_id, accelerator);

  if (result == AcceleratorConfigResult::kSuccess) {
    UpdateAndNotifyAccelerators();
  }

  VLOG(1) << "RemovedAccelerator called for ActionID: " << action_id
          << ", Accelerator: " << accelerator.GetShortcutText()
          << " returned: " << static_cast<int>(result);
  return result;
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
  Initialize(GetDefaultAccelerators());
  InitializeDeprecatedAccelerators();
}

void AshAcceleratorConfiguration::Initialize(
    base::span<const AcceleratorData> accelerators) {
  accelerators_.clear();
  deprecated_accelerators_.clear();
  id_to_accelerators_.clear();
  accelerator_to_id_.Clear();
  default_accelerators_to_id_cache_.Clear();
  default_id_to_accelerators_cache_.clear();

  // Cache these accelerators as the default.
  SetLookupMaps(accelerators, default_id_to_accelerators_cache_,
                default_accelerators_to_id_cache_);

  // TODO(jimmyxgong): Before adding the accelerators to the mappings, apply
  // pref remaps.
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

void AshAcceleratorConfiguration::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AshAcceleratorConfiguration::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
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
  SetLookupMaps(accelerators, id_to_accelerators_, accelerator_to_id_);
  UpdateAndNotifyAccelerators();
}

AcceleratorConfigResult AshAcceleratorConfiguration::DoRemoveAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  DCHECK(::features::IsShortcutCustomizationEnabled());

  AcceleratorAction* found_id = accelerator_to_id_.Find(accelerator);
  auto found_accelerators_iter = id_to_accelerators_.find(action_id);
  if (found_accelerators_iter == id_to_accelerators_.end() || !found_id) {
    return AcceleratorConfigResult::kNotFound;
  }

  DCHECK(*found_id == action_id);

  // Remove accelerator from lookup map.
  base::Erase(found_accelerators_iter->second, accelerator);

  // Remove accelerator from reverse lookup map.
  accelerator_to_id_.Erase(accelerator);

  return AcceleratorConfigResult::kSuccess;
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

void AshAcceleratorConfiguration::NotifyAcceleratorsUpdated() {
  if (!::features::IsShortcutCustomizationEnabled()) {
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnAcceleratorsUpdated();
  }
}

absl::optional<AcceleratorAction>
AshAcceleratorConfiguration::GetIdForDefaultAccelerator(
    ui::Accelerator accelerator) {
  AcceleratorAction* found_id =
      default_accelerators_to_id_cache_.Find(accelerator);
  return found_id ? absl::optional<AcceleratorAction>(*found_id)
                  : absl::nullopt;
}

std::vector<ui::Accelerator>
AshAcceleratorConfiguration::GetDefaultAcceleratorsForId(
    AcceleratorActionId id) {
  const auto iter = default_id_to_accelerators_cache_.find(id);
  DCHECK(iter != default_id_to_accelerators_cache_.end());

  return iter->second;
}

void AshAcceleratorConfiguration::UpdateAndNotifyAccelerators() {
  accelerators_.clear();
  accelerators_.reserve(accelerator_to_id_.size());
  for (const auto& [accel, action_id] : accelerator_to_id_) {
    accelerators_.push_back(accel);
  }

  UpdateAccelerators(id_to_accelerators_);
  NotifyAcceleratorsUpdated();
}

}  // namespace ash
