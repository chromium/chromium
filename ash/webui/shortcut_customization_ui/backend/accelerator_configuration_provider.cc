// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "ash/accelerators/accelerator_alias_converter.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
          {ui::KeyboardCode::VKEY_SPACE, u"Space"},
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

mojom::AcceleratorType GetAcceleratorType(ui::Accelerator accelerator) {
  // TODO(longbowei): Add and handle more Accelerator types in the future.
  if (Shell::Get()->ash_accelerator_configuration()->IsDeprecated(
          accelerator)) {
    return mojom::AcceleratorType::kDeprecated;
  }
  return mojom::AcceleratorType::kDefault;
}

// Create accelerator info using accelerator and extra properties.
mojom::AcceleratorInfoPtr CreateStandardAcceleratorInfo(
    const ui::Accelerator& accelerator,
    bool locked,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state) {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->locked = locked;
  info_mojom->type = type;
  info_mojom->state = state;
  info_mojom->layout_properties =
      mojom::LayoutStyleProperties::NewStandardAccelerator(
          CreateStandardAcceleratorProps(accelerator));

  return info_mojom;
}

// Create base accelerator info using accelerator.
mojom::AcceleratorInfoPtr CreateBaseAcceleratorInfo(
    const ui::Accelerator& accelerator) {
  // TODO(longbowei): Some accelerators should not be locked when customization
  // is allowed.
  return CreateStandardAcceleratorInfo(accelerator, /*locked=*/true,
                                       GetAcceleratorType(accelerator),
                                       mojom::AcceleratorState::kEnabled);
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

  // Observe top row keys are f-keys preference changes.
  Shell::Get()->keyboard_capability()->AddObserver(this);

  ash_accelerator_configuration_->AddAcceleratorsUpdatedCallback(
      base::BindRepeating(
          &AcceleratorConfigurationProvider::OnAcceleratorsUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  UpdateKeyboards();
  InitializeNonConfigurableAccelerators(GetNonConfigurableActionsMap());

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
  Shell::Get()->keyboard_capability()->RemoveObserver(this);
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

void AcceleratorConfigurationProvider::OnTopRowKeysAreFKeysChanged() {
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

void AcceleratorConfigurationProvider::UpdateKeyboards() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  DCHECK(device_data_manager);

  connected_keyboards_ = device_data_manager->GetKeyboardDevices();
  NotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::InitializeNonConfigurableAccelerators(
    NonConfigurableActionsMap mapping) {
  non_configurable_actions_mapping_ = std::move(mapping);
  for (const auto& [ambient_action_id, accelerators_details] :
       non_configurable_actions_mapping_) {
    if (accelerators_details.IsStandardAccelerator()) {
      DCHECK(!accelerators_details.replacements.has_value());
      DCHECK(!accelerators_details.message_id.has_value());
      for (const auto& accelerator :
           accelerators_details.accelerators.value()) {
        const uint32_t action_id = static_cast<uint32_t>(ambient_action_id);
        non_configurable_accelerator_to_id_.InsertNew(
            std::make_pair(accelerator, action_id));
        id_to_non_configurable_accelerators_[action_id].push_back(accelerator);
      }
    }
  }
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

std::vector<mojom::AcceleratorInfoPtr>
AcceleratorConfigurationProvider::CreateAcceleratorInfos(
    const std::vector<ui::Accelerator>& accelerators) const {
  std::vector<mojom::AcceleratorInfoPtr> infos_mojom;
  for (const auto& accelerator : accelerators) {
    // Get the alias accelerators by doing F-Keys remapping and
    // (reversed) six-pack-keys remapping if applicable.
    std::vector<ui::Accelerator> accelerator_aliases =
        accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
    for (const auto& accelerator_alias : accelerator_aliases) {
      infos_mojom.push_back(CreateBaseAcceleratorInfo(accelerator_alias));
    }
  }
  return infos_mojom;
}

mojom::TextAcceleratorPropertiesPtr
AcceleratorConfigurationProvider::CreateTextAcceleratorProperties(
    const NonConfigurableAcceleratorDetails& details) {
  DCHECK(details.message_id.has_value());
  // Ambient accelerators that only contain plain text e.g., Drag the
  // link to the tab's address bar.
  if (!details.replacements.has_value() || details.replacements->empty()) {
    std::vector<mojom::TextAcceleratorPartPtr> parts;
    parts.push_back(mojom::TextAcceleratorPart::New(
        l10n_util::GetStringUTF16(details.message_id.value()),
        mojom::TextAcceleratorPartType::kPlainText));
    return mojom::TextAcceleratorProperties::New(std::move(parts));
  }

  // Contains the start points of the replaced strings.
  std::vector<size_t> offsets;
  const std::vector<std::u16string> empty_string_replacements(
      details.replacements.value().size());
  // Pass an array of empty strings to get the offsets of the replacements. The
  // return string has the placeholders removed.
  const auto replaced_string = l10n_util::GetStringFUTF16(
      details.message_id.value(), empty_string_replacements, &offsets);

  // Sort the offsets and split the string on the offsets.
  sort(offsets.begin(), offsets.end());
  const auto plain_text_parts = SplitStringOnOffsets(replaced_string, offsets);

  auto text_accelerator_parts = GenerateTextAcceleratorParts(
      plain_text_parts, details.replacements.value(), offsets,
      replaced_string.size());
  return mojom::TextAcceleratorProperties::New(
      std::move(text_accelerator_parts));
}

mojom::AcceleratorInfoPtr
AcceleratorConfigurationProvider::CreateTextAcceleratorInfo(
    const NonConfigurableAcceleratorDetails& details) {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->locked = true;
  info_mojom->type = mojom::AcceleratorType::kDefault;
  info_mojom->state = mojom::AcceleratorState::kEnabled;
  info_mojom->layout_properties =
      mojom::LayoutStyleProperties::NewTextAccelerator(
          CreateTextAcceleratorProperties(details));
  return info_mojom;
}

AcceleratorConfigurationProvider::AcceleratorConfigurationMap
AcceleratorConfigurationProvider::CreateConfigurationMap() {
  AcceleratorConfigurationMap accelerator_config;
  // For each source, create a mapping between <ActionId, AcceleratorInfoPtr>.
  for (const auto& [source, id_to_accelerators] : accelerators_mapping_) {
    base::flat_map<AcceleratorActionId, std::vector<mojom::AcceleratorInfoPtr>>
        accelerators_mojom;
    for (const auto& [action_id, accelerators] : id_to_accelerators) {
      accelerators_mojom.emplace(action_id,
                                 CreateAcceleratorInfos(accelerators));
    }
    accelerator_config.emplace(source, std::move(accelerators_mojom));
  }

  // Add non-configuarable accelerators.
  ActionIdToAcceleratorsInfoMap non_configurable_accelerators;
  for (const auto& [ambient_action_id, accelerators_details] :
       non_configurable_actions_mapping_) {
    if (accelerators_details.IsStandardAccelerator()) {
      // These properties should only be set for text based layout accelerators
      DCHECK(!accelerators_details.replacements.has_value());
      DCHECK(!accelerators_details.message_id.has_value());
      non_configurable_accelerators.emplace(
          ambient_action_id,
          CreateAcceleratorInfos(accelerators_details.accelerators.value()));
    } else {
      // This property should only be set for standard accelerators
      DCHECK(!accelerators_details.accelerators.has_value());
      // For text-based layout accelerators, we always expect this to be a
      // vector with a single element.
      std::vector<mojom::AcceleratorInfoPtr> text_accelerators_info;
      text_accelerators_info.push_back(
          CreateTextAcceleratorInfo(accelerators_details));
      non_configurable_accelerators.emplace(ambient_action_id,
                                            std::move(text_accelerators_info));
    }
  }
  accelerator_config.emplace(mojom::AcceleratorSource::kAmbient,
                             std::move(non_configurable_accelerators));
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
