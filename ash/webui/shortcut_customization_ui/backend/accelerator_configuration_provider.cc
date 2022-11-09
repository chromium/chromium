// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <vector>

#include "ash/accelerators/accelerator_layout_table.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

mojom::AcceleratorInfoPtr CreateAcceleratorInfo(
    const ui::Accelerator& accelerator,
    bool locked,
    bool has_key_event,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state) {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->accelerator = accelerator;
  info_mojom->key_display = KeycodeToKeyString(accelerator.key_code());
  info_mojom->locked = locked;
  info_mojom->has_key_event = has_key_event;
  info_mojom->type = type;
  info_mojom->state = state;

  return info_mojom;
}

mojom::AcceleratorLayoutInfoPtr LayoutInfoToMojom(
    AcceleratorActionId action_id,
    AcceleratorLayoutDetails layout_details) {
  mojom::AcceleratorLayoutInfoPtr layout_info =
      mojom::AcceleratorLayoutInfo::New();
  layout_info->category = layout_details.category;
  layout_info->sub_category = layout_details.sub_category;
  layout_info->description =
      l10n_util::GetStringUTF16(kAcceleratorActionToStringIdMap.at(action_id));
  layout_info->style = layout_details.layout_style;
  layout_info->source = mojom::AcceleratorSource::kAsh;
  layout_info->action = static_cast<uint32_t>(action_id);

  return layout_info;
}

}  // namespace

namespace shortcut_ui {

AcceleratorConfigurationProvider::AcceleratorConfigurationProvider()
    : ash_accelerator_configuration_(
          Shell::Get()->ash_accelerator_configuration()) {
  // Observe connected keyboard events.
  ui::DeviceDataManager::GetInstance()->AddObserver(this);

  ash_accelerator_configuration_->AddAcceleratorsUpdatedCallback(
      base::BindRepeating(
          &AcceleratorConfigurationProvider::OnAcceleratorsUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  UpdateKeyboards();

  // Create LayoutInfos from kAcceleratorLayouts map. LayoutInfos are static
  // data that provides additional details for the app for styling.
  for (const auto& [action_id, layout_details] : kAcceleratorLayouts) {
    layout_infos_.push_back(LayoutInfoToMojom(action_id, layout_details));
  }
}

AcceleratorConfigurationProvider::~AcceleratorConfigurationProvider() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void AcceleratorConfigurationProvider::IsMutable(
    ash::mojom::AcceleratorSource source,
    IsMutableCallback callback) {
  if (source == ash::mojom::AcceleratorSource::kBrowser) {
    // Browser shortcuts are not mutable.
    std::move(callback).Run(/*is_mutable=*/false);
    return;
  }

  // TODO(jimmyxgong): Add more cases for other source types when they're
  // available.
  std::move(callback).Run(/*is_mutable=*/true);
}

void AcceleratorConfigurationProvider::GetAccelerators(
    GetAcceleratorsCallback callback) {
  std::move(callback).Run(CreateConfigurationMap());
}

void AcceleratorConfigurationProvider::AddObserver(
    mojo::PendingRemote<
        shortcut_customization::mojom::AcceleratorsUpdatedObserver> observer) {
  accelerators_updated_observers_.reset();
  accelerators_updated_observers_.Bind(std::move(observer));
}

void AcceleratorConfigurationProvider::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & (ui::InputDeviceEventObserver::kKeyboard)) {
    UpdateKeyboards();
  }
}

void AcceleratorConfigurationProvider::GetAcceleratorLayoutInfos(
    GetAcceleratorLayoutInfosCallback callback) {
  std::move(callback).Run(mojo::Clone(layout_infos_));
}

void AcceleratorConfigurationProvider::BindInterface(
    mojo::PendingReceiver<
        shortcut_customization::mojom::AcceleratorConfigurationProvider>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

mojom::AcceleratorType AcceleratorConfigurationProvider::GetAcceleratorType(
    ui::Accelerator accelerator) {
  // TODO(longbowei): Add and handle more Accelerator types in the future.
  if (ash_accelerator_configuration_->IsDeprecated(accelerator)) {
    return mojom::AcceleratorType::kDeprecated;
  }
  return mojom::AcceleratorType::kDefault;
}

void AcceleratorConfigurationProvider::UpdateKeyboards() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  DCHECK(device_data_manager);

  connected_keyboards_ = device_data_manager->GetKeyboardDevices();
  NotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::OnAcceleratorsUpdated(
    mojom::AcceleratorSource source,
    const ActionIdToAcceleratorsMap& mapping) {
  accelerators_mapping_[source] = mapping;
  NotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::NotifyAcceleratorsUpdated() {
  if (accelerators_updated_observers_.is_bound()) {
    accelerators_updated_observers_->OnAcceleratorsUpdated(
        CreateConfigurationMap());
  }
}

AcceleratorConfigurationProvider::AcceleratorConfigurationMap
AcceleratorConfigurationProvider::CreateConfigurationMap() {
  AcceleratorConfigurationMap accelerator_config;
  // For each source, create a mapping between <ActionId, AcceleratorInfoPtr>.
  for (const auto& [source, id_to_accelerators] : accelerators_mapping_) {
    base::flat_map<AcceleratorActionId, std::vector<mojom::AcceleratorInfoPtr>>
        accelerators_mojom;
    for (const auto& [action_id, accelerators] : id_to_accelerators) {
      std::vector<mojom::AcceleratorInfoPtr> infos_mojom;
      for (const auto& accelerator : accelerators) {
        // |locked| and and |has_key_event| are both default to true now.
        // For |locked|, ash accelerators should not be locked when
        // customization is allowed. For |has_key_event|, we need to determine
        // its state based off of a keyboard device id.
        mojom::AcceleratorInfoPtr info = CreateAcceleratorInfo(
            accelerator, /*locked=*/true,
            /*has_key_event=*/true, GetAcceleratorType(accelerator),
            mojom::AcceleratorState::kEnabled);
        infos_mojom.push_back(std::move(info));
      }
      accelerators_mojom.emplace(action_id, std::move(infos_mojom));
    }
    accelerator_config.emplace(source, std::move(accelerators_mojom));
  }
  return accelerator_config;
}

}  // namespace shortcut_ui
}  // namespace ash
