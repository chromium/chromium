// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <vector>

#include "ash/accelerators/accelerator_layout_table.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "components/prefs/pref_service.h"
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

// A map between Top row keys to Function keys.
// TODO(longbowei): This mapping is temporary, create a helper function in
// `ui/chromeos/events/keyboard_layout_util.h` to handle fetching the layout
// keys.
constexpr auto kLayout2TopRowKeyToFKeyMap =
    base::MakeFixedFlatMap<ui::KeyboardCode, ui::KeyboardCode>({
        {ui::KeyboardCode::VKEY_BROWSER_BACK, ui::KeyboardCode::VKEY_F1},
        {ui::KeyboardCode::VKEY_BROWSER_FORWARD, ui::KeyboardCode::VKEY_F2},
        {ui::KeyboardCode::VKEY_BROWSER_REFRESH, ui::KeyboardCode::VKEY_F3},
        {ui::KeyboardCode::VKEY_ZOOM, ui::KeyboardCode::VKEY_F4},
        {ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP1, ui::KeyboardCode::VKEY_F5},
        {ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN, ui::KeyboardCode::VKEY_F6},
        {ui::KeyboardCode::VKEY_BRIGHTNESS_UP, ui::KeyboardCode::VKEY_F7},
        {ui::KeyboardCode::VKEY_VOLUME_MUTE, ui::KeyboardCode::VKEY_F8},
        {ui::KeyboardCode::VKEY_VOLUME_DOWN, ui::KeyboardCode::VKEY_F9},
        {ui::KeyboardCode::VKEY_VOLUME_UP, ui::KeyboardCode::VKEY_F10},
    });

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

bool TopRowKeysAreFunctionKeys() {
  const PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service)
    return false;
  return pref_service->GetBoolean(prefs::kSendFunctionKeys);
}

bool IsTopRowKey(const ui::KeyboardCode& accelerator_keycode) {
  // A set that includes all top row keys from different keyboards.
  // TODO(longbowei): Now only include top row keys from layout2, add more top
  // row keys from other keyboards in the future.
  static const base::NoDestructor<base::flat_set<ui::KeyboardCode>>
      top_row_action_keys({
          ui::VKEY_BROWSER_BACK,
          ui::VKEY_BROWSER_REFRESH,
          ui::VKEY_ZOOM,
          ui::VKEY_MEDIA_LAUNCH_APP1,
          ui::VKEY_BRIGHTNESS_DOWN,
          ui::VKEY_BRIGHTNESS_UP,
          ui::VKEY_MEDIA_PLAY_PAUSE,
          ui::VKEY_VOLUME_MUTE,
          ui::VKEY_VOLUME_DOWN,
          ui::VKEY_VOLUME_UP,
      });
  return base::Contains(*top_row_action_keys, accelerator_keycode);
}

ui::Accelerator CreateTopRowRemappedAccelerator(ui::Accelerator accelerator) {
  if (TopRowKeysAreFunctionKeys() &&
      kLayout2TopRowKeyToFKeyMap.contains(accelerator.key_code())) {
    // If top row keys are function keys, top row shortcut will become
    // [Fkey] + [search] + [modifiers]
    ui::Accelerator new_accelerator(
        kLayout2TopRowKeyToFKeyMap.at(accelerator.key_code()),
        accelerator.modifiers() | ui::EF_COMMAND_DOWN, accelerator.key_state());
    return new_accelerator;
  }

  return accelerator;
}

ui::Accelerator CreateRemappedAccelerator(ui::Accelerator accelerator) {
  // TODO(longbowei): Add six-pack remapping in the next CL.
  if (IsTopRowKey(accelerator.key_code())) {
    return CreateTopRowRemappedAccelerator(accelerator);
  }

  return accelerator;
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
        // Update the accelerator by doing F-keys remapping
        // TODO(longbowei): Add six-pack remapping in next CL.
        ui::Accelerator updated_accelerator =
            CreateRemappedAccelerator(accelerator);

        // |locked| and and |has_key_event| are both default to true now.
        // For |locked|, ash accelerators should not be locked when
        // customization is allowed. For |has_key_event|, we need to determine
        // its state based off of a keyboard device id.
        mojom::AcceleratorInfoPtr info = CreateAcceleratorInfo(
            updated_accelerator, /*locked=*/true,
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
