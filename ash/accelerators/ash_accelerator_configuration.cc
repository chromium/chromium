// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "ui/base/accelerators/accelerator.h"

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

void AshAcceleratorConfiguration::InitializeAcceleratorMapping(
    base::span<const AcceleratorData> accelerators) {
  accelerator_infos_.clear();
  id_to_accelerator_infos_.clear();

  for (const auto& data : accelerators) {
    ui::Accelerator accelerator(data.keycode, data.modifiers);
    // TODO(jimmyxgong): Ash accelerators should not be locked when
    // customization is allowed.
    AcceleratorInfo info(ash::mojom::AcceleratorType::kDefault, accelerator,
                         /**locked=*/true);

    const uint32_t action_id = static_cast<uint32_t>(data.action);

    accelerator_infos_.push_back(info);
    id_to_accelerator_infos_[action_id].push_back(info);

    AddLayoutInfo(data);
  }
}

void AshAcceleratorConfiguration::AddLayoutInfo(const AcceleratorData& data) {
  // TODO(jimmyxgong): Replace with real i18n strings.
  std::u16string ash_category = u"Chrome OS";
  std::u16string system_sub_category = u"System";

  // TODO(jimmyxgong): This a basic stub implementation, replace with real
  // implementation.
  mojom::AcceleratorLayoutInfoPtr layout_info =
      mojom::AcceleratorLayoutInfo::New();
  layout_info->category = std::move(ash_category);
  layout_info->sub_category = std::move(system_sub_category);
  // TODO(jimmyxgong): Create a mapping between action_id and description.
  layout_info->description = u"Stub description";
  layout_info->style = mojom::AcceleratorLayoutStyle::kDefault;
  layout_info->source = mojom::AcceleratorSource::kAsh;
  layout_info->action = static_cast<uint32_t>(data.action);

  layout_infos_.push_back(std::move(layout_info));
}

}  // namespace ash
