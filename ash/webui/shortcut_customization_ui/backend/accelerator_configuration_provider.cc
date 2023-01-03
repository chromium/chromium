// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "ash/accelerators/accelerator_layout_table.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
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

std::vector<std::u16string> SplitStringOnOffsets(
    const std::u16string& input,
    const std::vector<size_t>& offsets) {
  DCHECK(std::is_sorted(offsets.begin(), offsets.end()));

  std::vector<std::u16string> parts;
  // At most there will be len(offsets) + 1 text parts.
  parts.reserve(offsets.size() + 1);
  size_t upto = 0;

  for (auto offset : offsets) {
    DCHECK_LE(offset, input.size());

    if (offset == upto) {
      continue;
    }

    DCHECK(offset >= upto);
    parts.push_back(input.substr(upto, offset - upto));
    upto = offset;
  }

  // Handles the case where there's plain text after the last replacement.
  if (upto < input.size()) {
    parts.push_back(input.substr(upto));
  }

  return parts;
}

std::vector<mojom::TextAcceleratorPartPtr> GenerateTextAcceleratorParts(
    const std::vector<std::u16string>& plain_text_parts,
    const std::vector<TextAcceleratorPart>& replacement_parts,
    const std::vector<size_t>& offsets,
    size_t str_size) {
  // |str_size| should be the sum of the lengths of |plain_text_parts|.
  DCHECK_EQ(str_size, std::accumulate(
                          plain_text_parts.begin(), plain_text_parts.end(), 0u,
                          [](size_t accumulator, const std::u16string& part) {
                            return accumulator + part.size();
                          }));

  DCHECK(std::is_sorted(offsets.begin(), offsets.end()));
  DCHECK_EQ(offsets.size(), replacement_parts.size());

  std::vector<mojom::TextAcceleratorPartPtr> result;
  size_t upto = 0;
  size_t offset_index = 0;
  size_t parts_index = 0;

  // Interleave the plain-text segments and the replacements based on the
  // offsets.
  while (upto < str_size || offset_index < offsets.size()) {
    // When there are still offsets remaining and the next available offset
    // |upto|, then add the next replacement to the result matches.
    if (offset_index < offsets.size() && upto == offsets[offset_index]) {
      const auto& replacement_part = replacement_parts[offset_index];
      result.push_back(mojom::TextAcceleratorPart::New(replacement_part.text,
                                                       replacement_part.type));
      offset_index++;
    } else {
      // Otherwise add the next plain text segment to the result.
      DCHECK(parts_index < plain_text_parts.size());
      const auto& plain_text_part = plain_text_parts[parts_index];
      result.push_back(mojom::TextAcceleratorPart::New(
          plain_text_part, mojom::TextAcceleratorPartType::kPlainText));

      upto += plain_text_part.size();
      parts_index++;
    }
  }

  DCHECK_EQ(upto, str_size);
  DCHECK_EQ(offset_index, offsets.size());
  return result;
}

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

mojom::StandardAcceleratorPropertiesPtr CreateStandardAcceleratorProps(
    const ui::Accelerator& accelerator) {
  return mojom::StandardAcceleratorProperties::New(
      accelerator, shortcut_ui::GetKeyDisplay(accelerator.key_code()));
}

mojom::AcceleratorLayoutInfoPtr LayoutInfoToMojom(
    AcceleratorLayoutDetails layout_details) {
  mojom::AcceleratorLayoutInfoPtr layout_info =
      mojom::AcceleratorLayoutInfo::New();
  layout_info->category = layout_details.category;
  layout_info->sub_category = layout_details.sub_category;
  layout_info->description =
      l10n_util::GetStringUTF16(layout_details.description_string_id);
  layout_info->style = layout_details.layout_style;
  layout_info->source = layout_details.source;
  layout_info->action = static_cast<uint32_t>(layout_details.action_id);

  return layout_info;
}

bool TopRowKeysAreFunctionKeys() {
  const PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service) {
    return false;
  }
  return pref_service->GetBoolean(prefs::kSendFunctionKeys);
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
AcceleratorConfigurationProvider::CreateTextAcceleratorProperties(
    const AcceleratorTextDetails& details) {
  // Contains the start points of the replaced strings.
  std::vector<size_t> offsets;
  const std::vector<std::u16string> empty_string_replacements(
      details.replacements.size());
  // Pass an array of empty strings to get the offsets of the replacements. The
  // return string has the placeholders removed.
  const auto replaced_string = l10n_util::GetStringFUTF16(
      details.message_id, empty_string_replacements, &offsets);

  // Sort the offsets and split the string on the offsets.
  sort(offsets.begin(), offsets.end());
  const auto plain_text_parts = SplitStringOnOffsets(replaced_string, offsets);

  auto text_accelerator_parts = GenerateTextAcceleratorParts(
      plain_text_parts, details.replacements, offsets, replaced_string.size());
  return mojom::TextAcceleratorProperties::New(
      std::move(text_accelerator_parts));
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
          CreateTextAcceleratorProperties(details));
  return info_mojom;
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateStandardAcceleratorInfo(
    const ui::Accelerator& accelerator,
    bool locked,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state) const {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->locked = locked;
  info_mojom->type = type;
  info_mojom->state = state;
  info_mojom->layout_properties =
      mojom::LayoutStyleProperties::NewStandardAccelerator(
          CreateStandardAcceleratorProps(accelerator));

  return info_mojom;
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateBaseAcceleratorInfo(
    const ui::Accelerator& accelerator) const {
  // TODO(longbowei): Some accelerators should not be locked when customization
  // is allowed.
  return CreateStandardAcceleratorInfo(accelerator, /*locked=*/true,
                                       GetAcceleratorType(accelerator),
                                       mojom::AcceleratorState::kEnabled);
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateRemappedTopRowAcceleratorInfo(
    const ui::Accelerator& accelerator) const {
  // Avoid remapping if [Search] is part of original accelerator.
  if (accelerator.IsCmdDown() || !TopRowKeysAreFunctionKeys() ||
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
  if (accelerator.IsCmdDown() ||
      !::features::IsImprovedKeyboardShortcutsEnabled() ||
      !ui::kSixPackKeyToSystemKeyMap.contains(accelerator.key_code())) {
    return nullptr;
  }
  // Edge cases:
  // 1. [Shift] + [Delete] should not be remapped to [Shift] + [Search] +
  // [Back] (aka, Insert).
  // 2. For [Insert], avoid remapping if [Shift] is part of original
  // accelerator.
  if (accelerator.IsShiftDown() &&
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
