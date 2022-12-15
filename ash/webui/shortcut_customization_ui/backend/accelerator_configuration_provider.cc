// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <string>
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
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

// This map is for KeyboardCodes that don't return a key_display from
// `KeycodeToKeyString`. The string values here were arbitrarily chosen
// based on the VKEY enum name.
// TODO(cambickel): In the future, consolidate this lookup table to be in the
// same location as the layout table.
const base::flat_map<ui::KeyboardCode, std::u16string>& GetKeyDisplayMap() {
  static auto key_display_map =
      base::NoDestructor(base::flat_map<ui::KeyboardCode, std::u16string>({
          {ui::KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE,
           u"MicrophoneMuteToggle"},
          {ui::KeyboardCode::VKEY_KBD_BACKLIGHT_TOGGLE,
           u"KeyboardBacklightToggle"},
          {ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_UP, u"KeyboardBrightnessUp"},
          {ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN,
           u"KeyboardBrightnessDown"},
          {ui::KeyboardCode::VKEY_SLEEP, u"Sleep"},
          {ui::KeyboardCode::VKEY_NEW, u"NewTab"},
          {ui::KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE,
           u"PrivacyScreenToggle"},
          {ui::KeyboardCode::VKEY_ALL_APPLICATIONS, u"OpenLauncher"},
          {ui::KeyboardCode::VKEY_DICTATE, u"ToggleDictation"},
          {ui::KeyboardCode::VKEY_WLAN, u"ToggleWifi"},
          {ui::KeyboardCode::VKEY_EMOJI_PICKER, u"EmojiPicker"},
          {ui::KeyboardCode::VKEY_SPACE,
           l10n_util::GetStringUTF16(IDS_SHORTCUT_CUSTOMIZATION_KEY_SPACE)},
          {ui::KeyboardCode::VKEY_TAB,
           l10n_util::GetStringUTF16(IDS_SHORTCUT_CUSTOMIZATION_KEY_TAB)},
          {ui::KeyboardCode::VKEY_ESCAPE,
           l10n_util::GetStringUTF16(IDS_SHORTCUT_CUSTOMIZATION_KEY_ESCAPE)},
          {ui::KeyboardCode::VKEY_RETURN,
           l10n_util::GetStringUTF16(IDS_SHORTCUT_CUSTOMIZATION_KEY_RETURN)},
          {ui::KeyboardCode::VKEY_BACK,
           l10n_util::GetStringUTF16(IDS_SHORTCUT_CUSTOMIZATION_KEY_BACKSPACE)},
      }));
  return *key_display_map;
}

mojom::DefaultAcceleratorPropertiesPtr CreateDefaultAcceleratorProps(
    const ui::Accelerator& accelerator) {
  return mojom::DefaultAcceleratorProperties::New(
      accelerator, shortcut_ui::GetKeyDisplay(accelerator.key_code()));
}

std::u16string LookupAcceleratorDescription(mojom::AcceleratorSource source,
                                            AcceleratorActionId action_id) {
  switch (source) {
    case mojom::AcceleratorSource::kAsh:
      return l10n_util::GetStringUTF16(
          kAcceleratorActionToStringIdMap.at(action_id));
    case mojom::AcceleratorSource::kAmbient:
      return l10n_util::GetStringUTF16(
          kAmbientActionToStringIdMap.at(action_id));
    // TODO(longbowei): Add strings for Browser shortcuts.
    case mojom::AcceleratorSource::kBrowser:
    case mojom::AcceleratorSource::kEventRewriter:
    case mojom::AcceleratorSource::kAndroid:
      NOTREACHED();
      return std::u16string();
  }
}

mojom::AcceleratorLayoutInfoPtr LayoutInfoToMojom(
    AcceleratorLayoutDetails layout_details) {
  mojom::AcceleratorLayoutInfoPtr layout_info =
      mojom::AcceleratorLayoutInfo::New();
  layout_info->category = layout_details.category;
  layout_info->sub_category = layout_details.sub_category;
  layout_info->description = LookupAcceleratorDescription(
      layout_details.source, layout_details.action_id);
  layout_info->style = layout_details.layout_style;
  layout_info->source = layout_details.source;
  layout_info->action = static_cast<uint32_t>(layout_details.action_id);

  return layout_info;
}

bool TopRowKeysAreFunctionKeys() {
  const PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service)
    return false;
  return pref_service->GetBoolean(prefs::kSendFunctionKeys);
}

// TODO(zhangwenyu): Remove this and use member function in ui::accelerator
// class.
bool IsModifierSet(const ui::Accelerator accelerator, int modifier) {
  return accelerator.modifiers() & modifier;
}

}  // namespace

namespace shortcut_ui {

AcceleratorConfigurationProvider::AcceleratorConfigurationProvider()
    : ash_accelerator_configuration_(
          Shell::Get()->ash_accelerator_configuration()) {
  // Observe connected keyboard events.
  ui::DeviceDataManager::GetInstance()->AddObserver(this);

  // Observe keyboard input method changes.
  input_method::InputMethodManager::Get()->AddObserver(this);

  ash_accelerator_configuration_->AddAcceleratorsUpdatedCallback(
      base::BindRepeating(
          &AcceleratorConfigurationProvider::OnAcceleratorsUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  UpdateKeyboards();
  InitializeNonConfigurableAccelerators(GetTextDetailsMap());

  // Create LayoutInfos from kAcceleratorLayouts. LayoutInfos are static
  // data that provides additional details for the app for styling.
  for (const auto& layout_details : kAcceleratorLayouts) {
    layout_infos_.push_back(LayoutInfoToMojom(layout_details));
  }
}

AcceleratorConfigurationProvider::~AcceleratorConfigurationProvider() {
  DCHECK(ui::DeviceDataManager::GetInstance());
  DCHECK(input_method::InputMethodManager::Get());

  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  input_method::InputMethodManager::Get()->RemoveObserver(this);
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

void AcceleratorConfigurationProvider::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  // Accelerators are updated to match the current input method, e.g. positional
  // shortcuts.
  NotifyAcceleratorsUpdated();
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
    ui::Accelerator accelerator) const {
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

void AcceleratorConfigurationProvider::InitializeNonConfigurableAccelerators(
    NonConfigurableActionsTextDetailsMap mapping) {
  non_configurable_actions_mapping_ = std::move(mapping);
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

mojom::TextAcceleratorPropertiesPtr
AcceleratorConfigurationProvider::CreateTextAcceleratorParts() {
  // TODO(michaelcheco): Use AcceleratorTextDetails to create
  // text_parts dynamically.
  std::vector<mojom::TextAcceleratorPartPtr> text_parts;
  text_parts.push_back(mojom::TextAcceleratorPart::New(
      u"ctrl", mojom::TextAcceleratorPartType::kModifier));
  text_parts.push_back(mojom::TextAcceleratorPart::New(
      u" + ", mojom::TextAcceleratorPartType::kPlainText));
  text_parts.push_back(mojom::TextAcceleratorPart::New(
      u"1 ", mojom::TextAcceleratorPartType::kKey));
  text_parts.push_back(mojom::TextAcceleratorPart::New(
      u"through", mojom::TextAcceleratorPartType::kPlainText));
  text_parts.push_back(mojom::TextAcceleratorPart::New(
      u"8", mojom::TextAcceleratorPartType::kKey));

  return mojom::TextAcceleratorProperties::New(std::move(text_parts));
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateTextAcceleratorInfo(
    const AcceleratorTextDetails& details) {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->locked = true;
  info_mojom->type = mojom::AcceleratorType::kDefault;
  info_mojom->state = mojom::AcceleratorState::kEnabled;
  info_mojom->layout_properties =
      mojom::LayoutStyleProperties::NewTextAccelerator(
          CreateTextAcceleratorParts());
  return info_mojom;
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateDefaultAcceleratorInfo(
    const ui::Accelerator& accelerator,
    bool locked,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state) const {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->locked = locked;
  info_mojom->type = type;
  info_mojom->state = state;
  info_mojom->layout_properties =
      mojom::LayoutStyleProperties::NewDefaultAccelerator(
          CreateDefaultAcceleratorProps(accelerator));

  return info_mojom;
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateBaseAcceleratorInfo(
    const ui::Accelerator& accelerator) const {
  // TODO(longbowei): Some accelerators should not be locked when customization
  // is allowed.
  return CreateDefaultAcceleratorInfo(accelerator, /*locked=*/true,
                                      GetAcceleratorType(accelerator),
                                      mojom::AcceleratorState::kEnabled);
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateRemappedTopRowAcceleratorInfo(
    const ui::Accelerator& accelerator) const {
  // Avoid remapping if [Search] is part of original accelerator.
  if (IsModifierSet(accelerator, ui::EF_COMMAND_DOWN) ||
      !TopRowKeysAreFunctionKeys() ||
      !ui::kLayout2TopRowKeyToFKeyMap.contains(accelerator.key_code())) {
    // No remapping is done.
    return nullptr;
  }
  // If top row keys are function keys, top row shortcut will become
  // [Fkey] + [search] + [modifiers]
  ui::Accelerator updated_accelerator(
      ui::kLayout2TopRowKeyToFKeyMap.at(accelerator.key_code()),
      accelerator.modifiers() | ui::EF_COMMAND_DOWN, accelerator.key_state());
  return CreateBaseAcceleratorInfo(updated_accelerator);
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateRemappedSixPackAcceleratorInfo(
    const ui::Accelerator& accelerator) const {
  // For all six-pack-keys, avoid remapping if [Search] is part of
  // original accelerator.
  if (IsModifierSet(accelerator, ui::EF_COMMAND_DOWN) ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::kSixPackKeyToSystemKeyMap.contains(accelerator.key_code())) {
    return nullptr;
  }
  // Edge cases:
  // 1. [Shift] + [Delete] should not be remapped to [Shift] + [Search] +
  // [Back] (aka, Insert).
  // 2. For [Insert], avoid remapping if [Shift] is part of original
  // accelerator.
  if (IsModifierSet(accelerator, ui::EF_SHIFT_DOWN) &&
      (accelerator.key_code() == ui::KeyboardCode::VKEY_DELETE ||
       accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT)) {
    return nullptr;
  }
  // For Insert: [modifiers] = [Search] + [Shift] + [original_modifiers].
  // For other six-pack-keys: [modifiers] = [Search] + [original_modifiers].
  int updated_modifiers =
      accelerator.key_code() == ui::KeyboardCode::VKEY_INSERT
          ? accelerator.modifiers() | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN
          : accelerator.modifiers() | ui::EF_COMMAND_DOWN;
  ui::Accelerator updated_accelerator =
      ui::Accelerator(ui::kSixPackKeyToSystemKeyMap.at(accelerator.key_code()),
                      updated_modifiers, accelerator.key_state());

  return CreateBaseAcceleratorInfo(updated_accelerator);
}

std::vector<mojom::AcceleratorInfoPtr>
AcceleratorConfigurationProvider::CreateAcceleratorInfoVariants(
    const ui::Accelerator& accelerator) const {
  std::vector<mojom::AcceleratorInfoPtr> alias_infos;

  if (Shell::Get()->keyboard_capability()->IsTopRowKey(
          accelerator.key_code())) {
    // For |top_row_key|, replace the base accelerator info with top-row
    // remapped accelerator info if remapping is done. Otherwise, only show base
    // accelerator info.
    if (auto result_ptr = CreateRemappedTopRowAcceleratorInfo(accelerator);
        result_ptr) {
      alias_infos.push_back(std::move(result_ptr));
      return alias_infos;
    }
  }

  if (Shell::Get()->keyboard_capability()->IsSixPackKey(
          accelerator.key_code())) {
    // For |six_pack_key|, show both the base accelerator info and the six-pack
    // remapped accelerator info if remapping is done. Otherwise, only show base
    // accelerator info.
    if (auto result_ptr = CreateRemappedSixPackAcceleratorInfo(accelerator);
        result_ptr) {
      alias_infos.push_back(std::move(result_ptr));
    }
  }

  // Add base accelerator info.
  alias_infos.push_back(CreateBaseAcceleratorInfo(accelerator));
  return alias_infos;
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
        // Get the alias acceleratorInfos by doing F-keys remapping and
        // six-pack-keys remapping if applicable.
        std::vector<mojom::AcceleratorInfoPtr> infos =
            CreateAcceleratorInfoVariants(accelerator);
        for (auto& info : infos) {
          infos_mojom.push_back(std::move(info));
        }
      }
      accelerators_mojom.emplace(action_id, std::move(infos_mojom));
    }
    accelerator_config.emplace(source, std::move(accelerators_mojom));
  }

  // Add non-configuarable accelerators.
  for (const auto& [ambient_action_id, accelerator_text_details] :
       non_configurable_actions_mapping_) {
    ActionIdToAcceleratorsInfoMap non_configurable_accelerators;
    // For text based layout accelerators, we always expect this to be a vector
    // with a single element.
    std::vector<mojom::AcceleratorInfoPtr> text_accelerators_info;
    text_accelerators_info.push_back(
        CreateTextAcceleratorInfo(accelerator_text_details));
    non_configurable_accelerators.emplace(ambient_action_id,
                                          std::move(text_accelerators_info));
    accelerator_config.emplace(mojom::AcceleratorSource::kAmbient,
                               std::move(non_configurable_accelerators));
  }
  return accelerator_config;
}

std::u16string GetKeyDisplay(ui::KeyboardCode key_code) {
  // If there's an entry for this key_code in our
  // map, return that entry's value.
  auto it = GetKeyDisplayMap().find(key_code);
  if (it != GetKeyDisplayMap().end()) {
    return it->second;
  } else {
    // Otherwise, get the key_display from a util function.
    return KeycodeToKeyString(key_code);
  }
}

}  // namespace shortcut_ui
}  // namespace ash
