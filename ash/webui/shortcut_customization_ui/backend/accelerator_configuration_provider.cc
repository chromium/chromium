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
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

using ::ash::shortcut_customization::mojom::AcceleratorResultData;
using ::ash::shortcut_customization::mojom::AcceleratorResultDataPtr;
using ::ash::shortcut_customization::mojom::SimpleAccelerator;
using ::ash::shortcut_customization::mojom::SimpleAcceleratorPtr;
using mojom::AcceleratorConfigResult;

// Gets the parts of the string that don't contain replacements.
// Ex: "Press and " -> ["Press ", " and "]
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

// Creates text accelerator parts needed to properly display kText accelerators
// in the UI. Uses the list of offsets which must be sorted and contains the
// start points of our replacements to place the |plain_text_parts| and
// |replacement_parts| in the correct order.
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

mojom::StandardAcceleratorPropertiesPtr CreateStandardAcceleratorProps(
    const ui::Accelerator& accelerator) {
  return mojom::StandardAcceleratorProperties::New(
      accelerator, ash::GetKeyDisplay(accelerator.key_code()));
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

  // In unit tests, the Shell instance may already be deleted at this point.
  if (Shell::HasInstance()) {
    Shell::Get()->keyboard_capability()->RemoveObserver(this);
  }
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

void AcceleratorConfigurationProvider::HasLauncherButton(
    HasLauncherButtonCallback callback) {
  std::move(callback).Run(
      Shell::Get()->keyboard_capability()->HasLauncherButton());
}

void AcceleratorConfigurationProvider::GetAccelerators(
    GetAcceleratorsCallback callback) {
  std::move(callback).Run(CreateConfigurationMap());
}

void AcceleratorConfigurationProvider::AddObserver(
    AcceleratorConfigurationProvider::AcceleratorsUpdatedObserver* observer) {
  accelerators_updated_observers_.AddObserver(observer);
}

void AcceleratorConfigurationProvider::RemoveObserver(
    AcceleratorConfigurationProvider::AcceleratorsUpdatedObserver* observer) {
  accelerators_updated_observers_.RemoveObserver(observer);
}

void AcceleratorConfigurationProvider::AddObserver(
    mojo::PendingRemote<
        shortcut_customization::mojom::AcceleratorsUpdatedObserver> observer) {
  accelerators_updated_mojo_observer_.reset();
  accelerators_updated_mojo_observer_.Bind(std::move(observer));
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

AcceleratorConfigurationProvider::AcceleratorConfigurationMap
AcceleratorConfigurationProvider::GetAcceleratorConfig() {
  return CreateConfigurationMap();
}

std::vector<mojom::AcceleratorLayoutInfoPtr>
AcceleratorConfigurationProvider::GetAcceleratorLayoutInfos() const {
  return mojo::Clone(layout_infos_);
}

void AcceleratorConfigurationProvider::GetAcceleratorLayoutInfos(
    GetAcceleratorLayoutInfosCallback callback) {
  std::move(callback).Run(mojo::Clone(layout_infos_));
}

void AcceleratorConfigurationProvider::RemoveAccelerator(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    const ui::Accelerator& accelerator,
    RemoveAcceleratorCallback callback) {
  DCHECK(::features::IsShortcutCustomizationEnabled());
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  if (source != mojom::AcceleratorSource::kAsh) {
    // Only ash accelerators can be modified, return early since all other
    // sources are locked.
    result_data->result = AcceleratorConfigResult::kActionLocked;
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Verify that `action_id` is a valid Ash accelerator ID. If validity checks
  // fail, return `kNotFound`.
  if (!ash_accelerator_configuration_->IsValid(action_id)) {
    result_data->result = AcceleratorConfigResult::kNotFound;
    std::move(callback).Run(std::move(result_data));
    return;
  }

  AcceleratorConfigResult result =
      ash_accelerator_configuration_->RemoveAccelerator(action_id, accelerator);
  result_data->result = result;
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::RestoreDefault(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    RestoreDefaultCallback callback) {
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  // Restoring an action is only supported for ash accelerators.
  if (source != mojom::AcceleratorSource::kAsh) {
    result_data->result = AcceleratorConfigResult::kActionLocked;
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Verify that `action_id` is a valid Ash accelerator ID. If validity checks
  // fail, return `kNotFound`.
  if (!ash_accelerator_configuration_->IsValid(action_id)) {
    result_data->result = AcceleratorConfigResult::kNotFound;
    std::move(callback).Run(std::move(result_data));
    return;
  }

  AcceleratorConfigResult result =
      ash_accelerator_configuration_->RestoreDefault(action_id);
  result_data->result = result;
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::RestoreAllDefaults(
    RestoreAllDefaultsCallback callback) {
  CHECK(::features::IsShortcutCustomizationEnabled());
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();
  AcceleratorConfigResult result =
      ash_accelerator_configuration_->RestoreAllDefaults();
  result_data->result = result;
  std::move(callback).Run(std::move(result_data));
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
  AcceleratorConfigurationMap config_map = CreateConfigurationMap();
  if (accelerators_updated_mojo_observer_.is_bound()) {
    accelerators_updated_mojo_observer_->OnAcceleratorsUpdated(
        mojo::Clone(config_map));
  }
  for (auto& observer : accelerators_updated_observers_) {
    observer.OnAcceleratorsUpdated(mojo::Clone(config_map));
  }
}

void AcceleratorConfigurationProvider::CreateAndAppendAliasedAccelerators(
    const ui::Accelerator& accelerator,
    bool locked,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state,
    std::vector<mojom::AcceleratorInfoPtr>& output) {
  // Get the alias accelerators by doing F-Keys remapping and
  // (reversed) six-pack-keys remapping if applicable.
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  output.reserve(output.size() + accelerator_aliases.size());

  for (const auto& accelerator_alias : accelerator_aliases) {
    output.push_back(CreateStandardAcceleratorInfo(
        accelerator_alias, locked, GetAcceleratorType(accelerator), state));
  }
}

mojom::TextAcceleratorPropertiesPtr
AcceleratorConfigurationProvider::CreateTextAcceleratorProperties(
    const NonConfigurableAcceleratorDetails& details) const {
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
    const NonConfigurableAcceleratorDetails& details) const {
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
  PopulateAshAcceleratorConfig(accelerator_config);
  PopulateAmbientAcceleratorConfig(accelerator_config);
  return accelerator_config;
}

void AcceleratorConfigurationProvider::PopulateAshAcceleratorConfig(
    AcceleratorConfigurationMap& accelerator_config_output) {
  const auto& id_to_accelerators =
      accelerators_mapping_.at(mojom::AcceleratorSource::kAsh);
  auto& output_action_id_to_accelerators =
      accelerator_config_output[mojom::AcceleratorSource::kAsh];

  for (const auto& layout_info : kAcceleratorLayouts) {
    if (layout_info.source != mojom::AcceleratorSource::kAsh) {
      // Only ash accelerators can have dynamically modified properties.
      // Note that ambient accelerators cannot be in kAsh.
      continue;
    }

    const auto& id_to_accelerator_iter =
        id_to_accelerators.find(layout_info.action_id);
    // For tests, we only want to test a subset of accelerators so it's possible
    // that we don't have accelerators for the given `layout_info`.
    if (id_to_accelerator_iter == id_to_accelerators.end() &&
        ignore_layouts_for_testing_) {
      continue;
    } else {
      DCHECK(id_to_accelerator_iter != id_to_accelerators.end());
    }

    const auto& accelerators = id_to_accelerator_iter->second;

    // Check if the default accelerators are available, if not re-add them but
    // mark them as disabled.
    const std::vector<ui::Accelerator>& default_accelerators =
        ash_accelerator_configuration_->GetDefaultAcceleratorsForId(
            layout_info.action_id);
    for (const auto& default_accelerator : default_accelerators) {
      if (base::Contains(accelerators, default_accelerator)) {
        continue;
      }

      // Append the missing default accelerators but marked as disabled by user.
      CreateAndAppendAliasedAccelerators(
          default_accelerator, layout_info.locked,
          mojom::AcceleratorType::kDefault,
          mojom::AcceleratorState::kDisabledByUser,
          output_action_id_to_accelerators[layout_info.action_id]);
    }

    for (const auto& accelerator : accelerators) {
      // TODO(jimmyxgong): Check pref storage to determine whether the
      // AcceleratorType was user-added or default.
      CreateAndAppendAliasedAccelerators(
          accelerator, layout_info.locked, mojom::AcceleratorType::kDefault,
          mojom::AcceleratorState::kEnabled,
          output_action_id_to_accelerators[layout_info.action_id]);
    }
  }
}

void AcceleratorConfigurationProvider::PopulateAmbientAcceleratorConfig(
    AcceleratorConfigurationMap& accelerator_config_output) {
  ActionIdToAcceleratorsInfoMap non_configurable_accelerators;
  for (const auto& [ambient_action_id, accelerators_details] :
       non_configurable_actions_mapping_) {
    if (accelerators_details.IsStandardAccelerator()) {
      // These properties should only be set for text based layout accelerators
      DCHECK(!accelerators_details.replacements.has_value());
      DCHECK(!accelerators_details.message_id.has_value());
      for (const auto& non_config_accelerator :
           accelerators_details.accelerators.value()) {
        CreateAndAppendAliasedAccelerators(
            non_config_accelerator,
            /*locked=*/true, mojom::AcceleratorType::kDefault,
            mojom::AcceleratorState::kEnabled,
            non_configurable_accelerators[ambient_action_id]);
      }
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
  accelerator_config_output.emplace(mojom::AcceleratorSource::kAmbient,
                                    std::move(non_configurable_accelerators));
}

}  // namespace shortcut_ui
}  // namespace ash
