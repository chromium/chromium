// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "ash/accelerators/accelerator_alias_converter.h"
#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_encoding.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/picker/picker_controller.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_configuration.mojom-shared.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

using ::ash::shortcut_customization::mojom::AcceleratorResultData;
using ::ash::shortcut_customization::mojom::AcceleratorResultDataPtr;
using ::ash::shortcut_customization::mojom::SimpleAccelerator;
using ::ash::shortcut_customization::mojom::SimpleAcceleratorPtr;
using ::ash::shortcut_customization::mojom::UserAction;
using mojom::AcceleratorConfigResult;
using HiddenAcceleratorMap =
    std::map<AcceleratorActionId, std::vector<ui::Accelerator>>;
using ReservedAcceleratorMap = std::map<ui::Accelerator, int>;

constexpr size_t kMaxAcceleratorsAllowed = 5;

constexpr char kShortcutCustomizationHistogramName[] =
    "Ash.ShortcutCustomization.CustomizationAction";
constexpr char kAddAcceleratorHistogramName[] =
    "Ash.ShortcutCustomization.AddAccelerator.";
constexpr char kRemoveDefaultAcceleratorHistogramName[] =
    "Ash.ShortcutCustomization.RemoveDefaultAccelerator.";

// Raw accelerator data may result in the same shortcut being displayed multiple
// times in the frontend. GetHiddenAcceleratorMap() is used to collect such
// accelerators and hide them from display.
const HiddenAcceleratorMap& GetHiddenAcceleratorMap() {
  static const auto kHiddenAcceleratorMap =
      base::NoDestructor<HiddenAcceleratorMap>({
          {AcceleratorAction::kToggleAppList,
           {ui::Accelerator(ui::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN,
                            ui::Accelerator::KeyState::PRESSED),
            ui::Accelerator(ui::VKEY_LWIN, ui::EF_SHIFT_DOWN,
                            ui::Accelerator::KeyState::RELEASED),
            ui::Accelerator(ui::VKEY_RWIN, ui::EF_NONE,
                            ui::Accelerator::KeyState::RELEASED),
            ui::Accelerator(ui::VKEY_RWIN, ui::EF_SHIFT_DOWN,
                            ui::Accelerator::KeyState::RELEASED)}},
          {AcceleratorAction::kToggleCapsLock,
           {ui::Accelerator(ui::VKEY_RWIN, ui::EF_ALT_DOWN,
                            ui::Accelerator::KeyState::RELEASED),
            ui::Accelerator(ui::VKEY_MENU, ui::EF_COMMAND_DOWN,
                            ui::Accelerator::KeyState::RELEASED)}},
          {AcceleratorAction::kShowShortcutViewer,
           {ui::Accelerator(ui::VKEY_F14, ui::EF_NONE,
                            ui::Accelerator::KeyState::PRESSED)}},
          {AcceleratorAction::kToggleFullscreen,
           {ui::Accelerator(ui::VKEY_ZOOM, ui::EF_SHIFT_DOWN,
                            ui::Accelerator::KeyState::PRESSED)}},
          {AcceleratorAction::kSwitchToLastUsedIme,
           {ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
                            ui::Accelerator::KeyState::RELEASED)}},
          {AcceleratorAction::kMediaPause,
           {ui::Accelerator(ui::VKEY_PAUSE, ui::EF_NONE,
                            ui::Accelerator::KeyState::PRESSED)}},
          {AcceleratorAction::kMediaPlay,
           {ui::Accelerator(ui::VKEY_PLAY, ui::EF_NONE,
                            ui::Accelerator::KeyState::PRESSED)}},
      });
  return *kHiddenAcceleratorMap;
}

constexpr int kCustomizationModifierMask =
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
    ui::EF_COMMAND_DOWN;

// The following are keys that are not allowed to be used as a customized
// activation key.
// VKEY_POWER, VKEY_SLEEP are related to power controls.
// VKEY_F13 is treated as lock on certain devices.
// VKEY_CAPITAL is capslock, capslock behavior is currently hardcoded and would
// lead to complications if we allow users to use it for their own accelerators.
// ScrollLock and NumLock are not officially supported on ChromeOS.
constexpr ui::KeyboardCode kReservedKeys[] = {
    ui::VKEY_POWER,   ui::VKEY_F13,    ui::VKEY_SLEEP,
    ui::VKEY_CAPITAL, ui::VKEY_SCROLL, ui::VKEY_NUMLOCK};

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
  DCHECK_EQ(str_size, std::transform_reduce(
                          plain_text_parts.begin(), plain_text_parts.end(), 0u,
                          std::plus<>(), [](const std::u16string& part) {
                            return part.length();
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
      if (replacement_part.keycode.has_value()) {
        // If the replacement is a `keycode`, we get the current key display
        // string. This can change dynamically based on the current user's
        // ime.
        result.push_back(mojom::TextAcceleratorPart::New(
            GetKeyDisplay(replacement_part.keycode.value()),
            replacement_part.type));
      } else {
        result.push_back(mojom::TextAcceleratorPart::New(
            replacement_part.text, replacement_part.type));
      }
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

// Hide accelerators if they are either:
// 1. In the `kHiddenAccelerators` map.
// 2. Not positionally remapped in the current keyboard layout, but are
//    positional remapped in the US layout. This is because these accelerators
//    would be impossible to utilize as the shortcut key_code is unable to be
//    produced in the current layout.
bool IsAcceleratorHidden(AcceleratorActionId action_id,
                         const ui::Accelerator& accelerator) {
  const auto& iter = GetHiddenAcceleratorMap().find(action_id);
  if (iter != GetHiddenAcceleratorMap().end()) {
    const std::vector<ui::Accelerator>& hidden_accelerators = iter->second;
    if (base::Contains(hidden_accelerators, accelerator)) {
      return true;
    }
  }

  auto keycode_entry =
      FindKeyCodeEntry(accelerator.key_code(), accelerator.code());
  if (!keycode_entry) {
    return false;
  }

  // Hide any accelerators which are not remapped in the current layout, but
  // are remapped in the us_layout.

  // As an example, the Semicolon key in the French (fr) keyboard layout is 'm'.
  // If I set a keyboard shortcut to Ctrl + ; in the us layout and then switch
  // to the fr layout, when I press Ctrl + m (; in us), the resulting key_code
  // will not correspond to Ctrl + ; in the US layout. This means the Ctrl + ;
  // shortcut must be hidden as it is not possible to trigger.
  const bool current_layout_vkey_is_remapped =
      ui::KeycodeConverter::MapPositionalDomCodeToUSShortcutKey(
          keycode_entry->dom_code, keycode_entry->resulting_key_code) !=
      ui::VKEY_UNKNOWN;
  const bool us_layout_vkey_is_remapped =
      ui::KeycodeConverter::MapUSPositionalShortcutKeyToDomCode(
          accelerator.key_code(), keycode_entry->dom_code) != ui::DomCode::NONE;
  return !current_layout_vkey_is_remapped && us_layout_vkey_is_remapped;
}

std::optional<std::u16string> GetReservedAcceleratorName(
    ui::Accelerator accelerator) {
  const auto iter = GetReservedAcceleratorsMap().find(accelerator);
  if (iter == GetReservedAcceleratorsMap().end()) {
    return std::nullopt;
  }
  return l10n_util::GetStringUTF16(iter->second);
}

mojom::StandardAcceleratorPropertiesPtr CreateStandardAcceleratorProps(
    const ui::Accelerator& accelerator,
    std::optional<ui::Accelerator> original_accelerator) {
  return mojom::StandardAcceleratorProperties::New(
      accelerator, ash::GetKeyDisplay(accelerator.key_code()),
      original_accelerator);
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
  if (Shell::Get()->ash_accelerator_configuration()->IsDeprecated(
          accelerator)) {
    return mojom::AcceleratorType::kDeprecated;
  }
  return mojom::AcceleratorType::kDefault;
}

// Create accelerator info using accelerator and extra properties.
mojom::AcceleratorInfoPtr CreateStandardAcceleratorInfo(
    const ui::Accelerator& accelerator,
    bool accelerator_locked,
    bool locked,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state,
    std::optional<ui::Accelerator> original_accelerator = std::nullopt) {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->accelerator_locked = accelerator_locked;
  info_mojom->locked = locked;
  info_mojom->type = type;
  info_mojom->state = state;
  info_mojom->layout_properties =
      mojom::LayoutStyleProperties::NewStandardAccelerator(
          CreateStandardAcceleratorProps(accelerator,
                                         std::move(original_accelerator)));

  return info_mojom;
}

// Returns a non-null value if there was an error detected with validating
// the `source` or `action_id`.
std::optional<AcceleratorConfigResult> ValidateSourceAndAction(
    mojom::AcceleratorSource source,
    AcceleratorActionId action_id,
    AshAcceleratorConfiguration* ash_accelerator_configuration) {
  // Adding/Removing an accelerator can only be done in Ash accelerators.
  if (source != mojom::AcceleratorSource::kAsh) {
    return AcceleratorConfigResult::kActionLocked;
  }

  // Verify that `action_id` is a valid Ash accelerator ID. If validity checks
  // fail, return `kNotFound`.
  if (!ash_accelerator_configuration->IsValid(action_id)) {
    return AcceleratorConfigResult::kNotFound;
  }

  return std::nullopt;
}

// Returns a non-null value if there was an error with validating the
// accelerator.
std::optional<AcceleratorConfigResult> ValidateAccelerator(
    const ui::Accelerator& accelerator) {
  // Sanitize the modifiers with only the relevant modifiers for customization.
  const int modifiers = accelerator.modifiers() & kCustomizationModifierMask;

  // Case: Accelerator must have Modifier + Key, unless the singular key
  // is a function key.
  if (modifiers == ui::EF_NONE &&
      !ui::KeyboardCapability::IsFunctionKey(accelerator.key_code())) {
    VLOG(1) << "Failed to validate accelerator: "
            << accelerator.GetShortcutText() << " with error: "
            << static_cast<int>(AcceleratorConfigResult::kMissingModifier);
    return AcceleratorConfigResult::kMissingModifier;
  }

  // Case: Reserved keys cannot be part of a custom accelerator.
  if (base::Contains(kReservedKeys, accelerator.key_code())) {
    VLOG(1) << "Failed to validate accelerator: "
            << accelerator.GetShortcutText() << " with error: "
            << static_cast<int>(AcceleratorConfigResult::kReservedKeyNotAllowed)
            << "- Reserved key in accelerator.";
    return AcceleratorConfigResult::kReservedKeyNotAllowed;
  }

  // Case: A function key accelerator cannot have the meta key modifier.
  if ((modifiers & ui::EF_COMMAND_DOWN) != 0 &&
      ui::KeyboardCapability::IsFunctionKey(accelerator.key_code())) {
    VLOG(1) << "Failed to validate accelerator: "
            << accelerator.GetShortcutText() << " with error: "
            << static_cast<int>(AcceleratorConfigResult::kKeyNotAllowed)
            << ". Accelerator has meta key with Function key.";
    return AcceleratorConfigResult::kSearchWithFunctionKeyNotAllowed;
  }

  // Case: Non-standard keys cannot have search as a modifier.
  std::optional<AcceleratorKeycodeLookupCache::KeyCodeLookupEntry>
      key_code_entry = FindKeyCodeEntry(accelerator.key_code());
  if (key_code_entry.has_value()) {
    const ui::KeyEvent key_event(
        ui::EventType::kKeyPressed, key_code_entry->resulting_key_code,
        key_code_entry->dom_code, accelerator.modifiers());
    const AcceleratorKeyInputType input_type =
        GetKeyInputTypeFromKeyEvent(key_event);
    if ((input_type == AcceleratorKeyInputType::kMisc ||
         input_type == AcceleratorKeyInputType::kTopRow) &&
        (modifiers & ui::EF_COMMAND_DOWN) != 0) {
      VLOG(1) << "Failed to validate accelerator: "
              << accelerator.GetShortcutText() << " with error: "
              << " Cannot have search with non-standard key.";
      return AcceleratorConfigResult::kNonStandardWithSearch;
    }
  }

  // Case: Top-row action keys cannot be part of the accelerator.
  std::optional<ui::TopRowActionKey> top_row_action_key =
      ui::KeyboardCapability::ConvertToTopRowActionKey(accelerator.key_code());
  if (top_row_action_key.has_value() &&
      Shell::Get()->keyboard_capability()->HasTopRowActionKeyOnAnyKeyboard(
          top_row_action_key.value())) {
    VLOG(1) << "Failed to validate accelerator: "
            << accelerator.GetShortcutText() << " with error: "
            << static_cast<int>(AcceleratorConfigResult::kKeyNotAllowed)
            << "- top row action key in accelerator.";
    return AcceleratorConfigResult::kKeyNotAllowed;
  }

  // Case: Accelerator cannot only have SHIFT as its modifier.
  if (modifiers == ui::EF_SHIFT_DOWN) {
    VLOG(1) << "Failed to validate accelerator: "
            << accelerator.GetShortcutText() << " with error: "
            << static_cast<int>(AcceleratorConfigResult::kShiftOnlyNotAllowed);
    return AcceleratorConfigResult::kShiftOnlyNotAllowed;
  }

  // Case: Accelerator cannot have right alt key.
  if (accelerator.key_code() == ui::VKEY_RIGHT_ALT) {
    VLOG(1) << "Failed to validate accelerator: "
            << accelerator.GetShortcutText() << " with error: "
            << static_cast<int>(AcceleratorConfigResult::kBlockRightAlt);
    return AcceleratorConfigResult::kBlockRightAlt;
  }

  // No errors with the accelerator.
  return std::nullopt;
}

std::string GetUuid(mojom::AcceleratorSource source,
                    AcceleratorActionId action) {
  return base::StrCat({base::NumberToString(static_cast<int>(source)), "-",
                       base::NumberToString(action)});
}

// Returns true if the given `details` should be excluded from the view, since
// certain shortcuts can be associated with a disabled feature behind a flag,
// or specific device property.
bool ShouldExcludeItem(const AcceleratorLayoutDetails& details) {
  switch (details.action_id) {
    // Hide user switching shortcuts for lacros builds.
    case kSwitchToNextUser:
    case kSwitchToPreviousUser:
      return crosapi::lacros_startup_state::IsLacrosEnabled();
    case kPrivacyScreenToggle:
      return accelerators::CanTogglePrivacyScreen();
    case kTilingWindowResizeLeft:
    case kTilingWindowResizeRight:
    case kTilingWindowResizeUp:
    case kTilingWindowResizeDown:
      return !features::IsTilingWindowResizeEnabled();
    case kToggleMouseKeys:
      return !::features::IsAccessibilityMouseKeysEnabled();
    case kToggleSnapGroupWindowsMinimizeAndRestore:
      return true;
    case kTogglePicker:
      return !(ash::features::IsPickerUpdateEnabled() &&
               Shell::Get()->picker_controller());
  }

  return false;
}

size_t GetNumOriginalAccelerators(
    std::vector<mojom::AcceleratorInfoPtr> infos) {
  size_t num_original_accelerators = 0;
  for (const auto& info : infos) {
    // If a standard accelerator has an `original_accelerator` value then it is
    // a generated aliased accelerator. Do not count it as part of the original
    // number of accelerators.
    // Ignore disabled accelerators as counting towards the maximum.
    if (info->layout_properties->is_standard_accelerator()) {
      if (info->layout_properties->get_standard_accelerator()
              ->original_accelerator.has_value() ||
          info->state != mojom::AcceleratorState::kEnabled) {
        continue;
      }
    }
    ++num_original_accelerators;
  }

  return num_original_accelerators;
}

// Update key_state for special trigger_on_release case.
ui::Accelerator ModifyKeyStateConditionally(
    const ui::Accelerator& accelerator) {
  // From kAcceleratorData(ash/public/cpp/accelerators.cc), kToggleAppList is
  // triggered on release so it needs to be special cased.
  if (accelerator.key_code() == ui::VKEY_LWIN &&
      accelerator.modifiers() == ui::EF_NONE) {
    // Create a new accelerator with key state set to RELEASED.
    ui::Accelerator updated_accelerator(accelerator.key_code(),
                                        accelerator.modifiers());
    updated_accelerator.set_key_state(ui::Accelerator::KeyState::RELEASED);
    return updated_accelerator;
  }

  return accelerator;
}

void LogReplaceAccelerator(mojom::AcceleratorSource source,
                           const ui::Accelerator& old_accelerator,
                           const ui::Accelerator& new_accelerator,
                           mojom::AcceleratorConfigResult error) {
  VLOG(1) << "ReplaceAccelerator returned for source: " << source
          << " old accelerator: " << old_accelerator.GetShortcutText()
          << " new_accelerator: " << new_accelerator.GetShortcutText()
          << " with error code: " << error;
}

void LogRemoveAccelerator(mojom::AcceleratorSource source,
                          const ui::Accelerator& accelerator,
                          mojom::AcceleratorConfigResult error) {
  VLOG(1) << "RemoveAccelerator returned for source: " << source
          << " accelerator: " << accelerator.GetShortcutText()
          << " with error: " << error;
}

void LogAddAccelerator(mojom::AcceleratorSource source,
                       const ui::Accelerator& new_accelerator,
                       mojom::AcceleratorConfigResult error) {
  VLOG(1) << "AddAccelerator returned for source: " << source
          << " accelerator: " << new_accelerator.GetShortcutText()
          << " with error: " << error;
}

void LogRestoreDefault(uint32_t action_id,
                       mojom::AcceleratorConfigResult error) {
  VLOG(1) << "RestoreDefault called for action ID: " << action_id
          << " with error code: " << error;
}

void RecordEncodedAcceleratorHistogram(const std::string& histogram_name,
                                       uint32_t action_id,
                                       const ui::Accelerator& accelerator) {
  base::UmaHistogramSparse(
      base::StrCat(
          {histogram_name, GetAcceleratorActionName(
                               static_cast<AcceleratorAction>(action_id))}),
      GetEncodedShortcut(accelerator.modifiers(), accelerator.key_code()));
}

}  // namespace

namespace shortcut_ui {

AcceleratorConfigurationProvider::AcceleratorConfigurationProvider(
    PrefService* pref_service)
    : ash_accelerator_configuration_(
          Shell::Get()->ash_accelerator_configuration()),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // Observe keyboard input method changes.
  input_method::InputMethodManager::Get()->AddObserver(this);

  // Observe shortcut policy changes.
  // Gets removed on `AcceleratorPrefs` destruction.
  if (Shell::Get()->accelerator_prefs()->IsUserEnterpriseManaged()) {
    Shell::Get()->accelerator_prefs()->AddObserver(this);
  }

  if (features::IsInputDeviceSettingsSplitEnabled()) {
    // `InputDeviceSettingsController` provides updates whenever a device is
    // connected/disconnected or if its settings changed. In any of these cases,
    // accelerators must be updated.
    // Observer is removed on destruction of `InputDeviceSettingsController`,
    // which happens before this class is destroyed.
    Shell::Get()->input_device_settings_controller()->AddObserver(this);
  } else {
    // Observe connected keyboard events.
    ui::DeviceDataManager::GetInstance()->AddObserver(this);

    send_function_keys_pref_ = std::make_unique<BooleanPrefMember>();
    send_function_keys_pref_->Init(
        ash::prefs::kSendFunctionKeys, pref_service,
        base::BindRepeating(&AcceleratorConfigurationProvider::
                                ScheduleNotifyAcceleratorsUpdated,
                            base::Unretained(this)));
  }

  ash_accelerator_configuration_->AddAcceleratorsUpdatedCallback(
      base::BindRepeating(
          &AcceleratorConfigurationProvider::OnAcceleratorsUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  UpdateKeyboards();

  // Create LayoutInfos from kAcceleratorLayouts. LayoutInfos are static
  // data that provides additional details for the app for styling.
  // Also create a cached shortcut description lookup.
  for (const auto& layout_id : kAcceleratorLayouts) {
    std::optional<AcceleratorLayoutDetails> layout =
        GetAcceleratorLayout(layout_id);
    if (!layout) {
      LOG(ERROR) << "Unexpectedly could not find layout for id: " << layout_id;
      continue;
    }
    if (ShouldExcludeItem(*layout)) {
      continue;
    }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (layout_id == AcceleratorAction::kTogglePicker &&
        Shell::Get()->keyboard_capability()->IsModifierSplitEnabled()) {
      layout->description_string_id = IDS_ASH_ACCELERATOR_DESCRIPTION_RIGHT_ALT;
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    layout_infos_.push_back(LayoutInfoToMojom(*layout));
    accelerator_layout_lookup_[GetUuid(layout->source, layout->action_id)] =
        *layout;
  }

  // Must initialize the non-configurable accelerators after the layout
  // has been set.
  InitializeNonConfigurableAccelerators(GetNonConfigurableActionsMap());
}

AcceleratorConfigurationProvider::~AcceleratorConfigurationProvider() {
  DCHECK(ui::DeviceDataManager::GetInstance());
  DCHECK(input_method::InputMethodManager::Get());

  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  // In unit tests, the Shell instance may already be deleted at this point.
  if (Shell::HasInstance()) {
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        /*prevent_processing_accelerators=*/false);
    if (features::IsInputDeviceSettingsSplitEnabled()) {
      Shell::Get()->input_device_settings_controller()->RemoveObserver(this);
    }
    if (Shell::Get()->accelerator_prefs()->IsUserEnterpriseManaged()) {
      Shell::Get()->accelerator_prefs()->RemoveObserver(this);
    }
  }
}

void AcceleratorConfigurationProvider::IsMutable(
    ash::mojom::AcceleratorSource source,
    IsMutableCallback callback) {
  bool is_mutable = false;
  switch (source) {
    case ash::mojom::AcceleratorSource::kAsh:
      is_mutable = ash_accelerator_configuration_->IsMutable();
      break;
    case ash::mojom::AcceleratorSource::kBrowser:
    case ash::mojom::AcceleratorSource::kAmbient:
    case ash::mojom::AcceleratorSource::kAndroid:
    case ash::mojom::AcceleratorSource::kEventRewriter:
      // The sources above are not mutable.
      break;
  }

  std::move(callback).Run(is_mutable);
}

void AcceleratorConfigurationProvider::IsCustomizationAllowedByPolicy(
    IsCustomizationAllowedByPolicyCallback callback) {
  // If not enterprise managed, return true.
  if (!Shell::Get()->accelerator_prefs()->IsUserEnterpriseManaged()) {
    std::move(callback).Run(/*is_customization_allowed_by_policy=*/true);
    return;
  }
  std::move(callback).Run(
      Shell::Get()->accelerator_prefs()->IsCustomizationAllowedByPolicy());
}

void AcceleratorConfigurationProvider::GetMetaKeyToDisplay(
    GetMetaKeyToDisplayCallback callback) {
  std::move(callback).Run(
      Shell::Get()->keyboard_capability()->GetMetaKeyToDisplay());
}

void AcceleratorConfigurationProvider::GetConflictAccelerator(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    const ui::Accelerator& accelerator,
    GetConflictAcceleratorCallback callback) {
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  // Validate the source and action.
  std::optional<AcceleratorConfigResult> error_result = ValidateSourceAndAction(
      source, action_id, ash_accelerator_configuration_);
  // `kActionLocked` from `ValidateSourceAndAction` indicates a non-ash source.
  // We still want to check the conflict in the case its from a non-ash source.
  if (error_result.has_value() &&
      *error_result != AcceleratorConfigResult::kActionLocked) {
    result_data->result = *error_result;
    std::move(callback).Run(std::move(result_data));
    VLOG(1) << "Attempted to add accelerator: " << accelerator.GetShortcutText()
            << " to an invalid source or action."
            << " source: " << static_cast<int>(source)
            << " action ID: " << action_id;
    return;
  }

  // Check if the accelerator is reserved. If so return an error.
  const std::optional<std::u16string> reserved_accelerator_name =
      GetReservedAcceleratorName(accelerator);
  if (reserved_accelerator_name.has_value()) {
    result_data->result = AcceleratorConfigResult::kConflict;
    result_data->shortcut_name = std::move(reserved_accelerator_name.value());
    std::move(callback).Run(std::move(result_data));
    VLOG(1) << " Attempted to add a reserved accelerator: "
            << accelerator.GetShortcutText();
    return;
  }

  // Check if `accelerator` conflicts with non-configurable accelerators.
  // This includes: browser, accessbility, and ambient accelerators.
  const std::vector<uint32_t> non_configurable_conflict_ids =
      FindNonConfigurableIdFromAccelerator(accelerator);
  // If there was a conflict with a non-configurable accelerator
  if (!non_configurable_conflict_ids.empty()) {
    result_data->result = AcceleratorConfigResult::kConflict;
    // Get the shortcut name and add it to the return struct.
    result_data->shortcut_name = l10n_util::GetStringUTF16(
        accelerator_layout_lookup_[GetUuid(
                                       mojom::AcceleratorSource::kAmbient,
                                       non_configurable_conflict_ids.front())]
            .description_string_id);
    std::move(callback).Run(std::move(result_data));
    VLOG(1) << "Attempted to add accelerator: " << accelerator.GetShortcutText()
            << " to a locked source: " << static_cast<int>(source);
    return;
  }

  // Check if the accelerator conflicts with an existing ash accelerator.
  const AcceleratorAction* found_ash_action =
      ash_accelerator_configuration_->FindAcceleratorAction(accelerator);

  // Conflict detected, return the conflict with an error.
  if (found_ash_action) {
    // At this point there is a conflict.
    result_data->result = AcceleratorConfigResult::kConflict;
    result_data->shortcut_name = l10n_util::GetStringUTF16(
        accelerator_layout_lookup_[GetUuid(mojom::AcceleratorSource::kAsh,
                                           *found_ash_action)]
            .description_string_id);
    std::move(callback).Run(std::move(result_data));
    VLOG(1) << "Conflict detected for attempting to add accelerator: "
            << accelerator.GetShortcutText() << " to action ID: " << action_id
            << ". With conflicts with action ID: " << *found_ash_action;
    return;
  }

  // No issues detected, return success.
  result_data->result = AcceleratorConfigResult::kSuccess;
  std::move(callback).Run(std::move(result_data));
  return;
}

// Get the default accelerators for the given accelerator id. The
// accelerators are filtered and aliased accelerators are included.
void AcceleratorConfigurationProvider::GetDefaultAcceleratorsForId(
    uint32_t action_id,
    GetDefaultAcceleratorsForIdCallback callback) {
  std::move(callback).Run(GetDefaultAcceleratorsForId(action_id));
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

void AcceleratorConfigurationProvider::AddPolicyObserver(
    mojo::PendingRemote<shortcut_customization::mojom::PolicyUpdatedObserver>
        observer) {
  policy_updated_mojo_observer.reset();
  policy_updated_mojo_observer.Bind(std::move(observer));
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
  ScheduleNotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::OnKeyboardConnected(
    const mojom::Keyboard& keyboard) {
  UpdateKeyboards();
}

void AcceleratorConfigurationProvider::OnKeyboardDisconnected(
    const mojom::Keyboard& keyboard) {
  UpdateKeyboards();
}

void AcceleratorConfigurationProvider::OnKeyboardSettingsUpdated(
    const mojom::Keyboard& keyboard) {
  ScheduleNotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::OnShortcutPolicyUpdated() {
  if (policy_updated_mojo_observer.is_bound()) {
    policy_updated_mojo_observer->OnCustomizationPolicyUpdated();
  }
  ScheduleNotifyAcceleratorsUpdated();
}

AcceleratorConfigurationProvider::AcceleratorConfigurationMap
AcceleratorConfigurationProvider::GetAcceleratorConfig() {
  return CreateConfigurationMap();
}

std::vector<mojom::AcceleratorLayoutInfoPtr>
AcceleratorConfigurationProvider::GetAcceleratorLayoutInfos() const {
  return mojo::Clone(layout_infos_);
}

void AcceleratorConfigurationProvider::PreventProcessingAccelerators(
    bool prevent_processing_accelerators,
    PreventProcessingAcceleratorsCallback callback) {
  // Always reset the pending accelerator whenever the user has just started
  // or stopped inputting an accelerator.
  pending_accelerator_.reset();
  conflict_error_state_ = AcceleratorConflictErrorState::kStandby;
  Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
      prevent_processing_accelerators);
  std::move(callback).Run();
}

void AcceleratorConfigurationProvider::GetAcceleratorLayoutInfos(
    GetAcceleratorLayoutInfosCallback callback) {
  std::move(callback).Run(mojo::Clone(layout_infos_));
}

void AcceleratorConfigurationProvider::AddAccelerator(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    const ui::Accelerator& accelerator,
    AddAcceleratorCallback callback) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  // Validate the source and action, if no errors then validate the accelerator.
  std::optional<AcceleratorConfigResult> error_result = ValidateSourceAndAction(
      source, action_id, ash_accelerator_configuration_);
  if (!error_result.has_value() &&
      !base::Contains(GetDefaultAcceleratorsForId(action_id), accelerator)) {
    error_result = ValidateAccelerator(accelerator);
  }

  if (error_result.has_value()) {
    pending_accelerator_.reset();
    result_data->result = *error_result;
    LogAddAccelerator(source, accelerator, result_data->result);
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Only allow a maximum of `kMaxAcceleratorsAllowed` per action.
  const auto& ash_accelerators_mapping =
      cached_configuration_.find(mojom::AcceleratorSource::kAsh);
  CHECK(ash_accelerators_mapping != cached_configuration_.end());

  const auto found_accelerator_infos =
      ash_accelerators_mapping->second.find(action_id);
  // Check that there is less than `kMaxAcceleratorsAllowed` accelerator infos
  // in the cached accelerator configuration mapping for `action_id`.
  if (found_accelerator_infos != ash_accelerators_mapping->second.end() &&
      GetNumOriginalAccelerators(mojo::Clone(
          found_accelerator_infos->second)) >= kMaxAcceleratorsAllowed) {
    result_data->result = AcceleratorConfigResult::kMaximumAcceleratorsReached;
    LogAddAccelerator(source, accelerator, result_data->result);
    std::move(callback).Run(std::move(result_data));
    return;
  }

  std::optional<AcceleratorResultDataPtr> result_data_ptr =
      PreprocessAddAccelerator(source, action_id, accelerator);
  // Check if there was an error during processing the accelerator, if so return
  // early with the error.
  if (result_data_ptr.has_value()) {
    std::move(callback).Run(std::move(*result_data_ptr));
    LogAddAccelerator(source, accelerator, result_data->result);
    return;
  }

  // After the accelerator has gone through conflict detection, check if the
  // accelerator contains search/meta.
  // Warn users that the inputted accelerator may conflict with existing
  // browser app shortcuts if there is no meta/search key as a modifier.
  conflict_error_state_ =
      MaybeHandleNonSearchAccelerator(accelerator, source, action_id);
  if (conflict_error_state_ != AcceleratorConflictErrorState::kStandby) {
    result_data->result = AcceleratorConfigResult::kNonSearchAcceleratorWarning;
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Continue with adding the accelerator.
  pending_accelerator_.reset();
  result_data->result = ash_accelerator_configuration_->AddUserAccelerator(
      action_id, accelerator);
  LogAddAccelerator(source, accelerator, result_data->result);
  base::UmaHistogramEnumeration(kShortcutCustomizationHistogramName,
                                ShortcutCustomizationAction::kAddAccelerator);
  RecordEncodedAcceleratorHistogram(kAddAcceleratorHistogramName, action_id,
                                    accelerator);
  base::UmaHistogramEnumeration(
      base::StrCat({"Ash.ShortcutCustomization.ModifyType.",
                    GetAcceleratorActionName(
                        static_cast<AcceleratorAction>(action_id))}),
      ModificationType::kAdd);
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::RemoveAccelerator(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    const ui::Accelerator& accelerator,
    RemoveAcceleratorCallback callback) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());
  ui::Accelerator accelerator_to_remove =
      ModifyKeyStateConditionally(accelerator);
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  std::optional<AcceleratorConfigResult> validated_source_action_result =
      ValidateSourceAndAction(source, action_id,
                              ash_accelerator_configuration_);
  if (validated_source_action_result.has_value()) {
    result_data->result = *validated_source_action_result;
    LogRemoveAccelerator(source, accelerator_to_remove, result_data->result);
    std::move(callback).Run(std::move(result_data));
    return;
  }

  AcceleratorConfigResult result =
      ash_accelerator_configuration_->RemoveAccelerator(action_id,
                                                        accelerator_to_remove);
  result_data->result = result;
  LogRemoveAccelerator(source, accelerator_to_remove, result_data->result);
  base::UmaHistogramEnumeration(
      kShortcutCustomizationHistogramName,
      ShortcutCustomizationAction::kRemoveAccelerator);
  base::UmaHistogramEnumeration(
      base::StrCat({"Ash.ShortcutCustomization.ModifyType.",
                    GetAcceleratorActionName(
                        static_cast<AcceleratorAction>(action_id))}),
      ModificationType::kRemove);

  // Only record this metric if the removed accelerator is a default accelerator
  // for `action_id`.
  std::optional<AcceleratorAction> default_id =
      ash_accelerator_configuration_->GetIdForDefaultAccelerator(accelerator);
  if (default_id == action_id) {
    RecordEncodedAcceleratorHistogram(kRemoveDefaultAcceleratorHistogramName,
                                      action_id, accelerator);
  }
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::ReplaceAccelerator(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    const ui::Accelerator& old_accelerator,
    const ui::Accelerator& new_accelerator,
    ReplaceAcceleratorCallback callback) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());

  ui::Accelerator accelerator_to_replace =
      ModifyKeyStateConditionally(old_accelerator);

  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  std::optional<AcceleratorConfigResult> error_result = ValidateSourceAndAction(
      source, action_id, ash_accelerator_configuration_);
  if (!error_result.has_value() &&
      !base::Contains(GetDefaultAcceleratorsForId(action_id),
                      new_accelerator)) {
    error_result = ValidateAccelerator(new_accelerator);
  }

  if (error_result.has_value()) {
    result_data->result = *error_result;
    LogReplaceAccelerator(source, accelerator_to_replace, new_accelerator,
                          result_data->result);
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Verify old accelerator exists.
  const AcceleratorAction* old_accelerator_id =
      ash_accelerator_configuration_->FindAcceleratorAction(
          accelerator_to_replace);
  if (!old_accelerator_id || *old_accelerator_id != action_id) {
    result_data->result = AcceleratorConfigResult::kNotFound;
    LogReplaceAccelerator(source, accelerator_to_replace, new_accelerator,
                          result_data->result);
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Check if there was an error during processing the accelerator, if so return
  // early with the error.
  std::optional<AcceleratorResultDataPtr> result_data_ptr =
      PreprocessAddAccelerator(source, action_id, new_accelerator);
  if (result_data_ptr.has_value()) {
    LogReplaceAccelerator(source, accelerator_to_replace, new_accelerator,
                          (*result_data_ptr)->result);
    std::move(callback).Run(std::move(*result_data_ptr));
    return;
  }

  // Warn users that the inputted accelerator may conflict with existing
  // app shortcuts if there is no meta/search key as a modifier.
  conflict_error_state_ =
      MaybeHandleNonSearchAccelerator(new_accelerator, source, action_id);
  if (conflict_error_state_ != AcceleratorConflictErrorState::kStandby) {
    result_data->result = AcceleratorConfigResult::kNonSearchAcceleratorWarning;
    std::move(callback).Run(std::move(result_data));
    return;
  }

  // Continue with replacing the accelerator.
  pending_accelerator_.reset();
  result_data->result = ash_accelerator_configuration_->ReplaceAccelerator(
      action_id, accelerator_to_replace, new_accelerator);
  LogReplaceAccelerator(source, accelerator_to_replace, new_accelerator,
                        result_data->result);

  base::UmaHistogramEnumeration(
      kShortcutCustomizationHistogramName,
      ShortcutCustomizationAction::kReplaceAccelerator);
  RecordEncodedAcceleratorHistogram(kAddAcceleratorHistogramName, action_id,
                                    new_accelerator);
  base::UmaHistogramEnumeration(
      base::StrCat({"Ash.ShortcutCustomization.ModifyType.",
                    GetAcceleratorActionName(
                        static_cast<AcceleratorAction>(action_id))}),
      ModificationType::kEdit);
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::RestoreDefault(
    mojom::AcceleratorSource source,
    uint32_t action_id,
    RestoreDefaultCallback callback) {
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  std::optional<AcceleratorConfigResult> validated_source_action_result =
      ValidateSourceAndAction(source, action_id,
                              ash_accelerator_configuration_);
  if (validated_source_action_result.has_value()) {
    result_data->result = *validated_source_action_result;
    LogRestoreDefault(action_id, result_data->result);
    std::move(callback).Run(std::move(result_data));
    return;
  }

  AcceleratorConfigResult result =
      ash_accelerator_configuration_->RestoreDefault(action_id);
  result_data->result = result;
  base::UmaHistogramEnumeration(kShortcutCustomizationHistogramName,
                                ShortcutCustomizationAction::kResetAction);
  base::UmaHistogramEnumeration(
      base::StrCat({"Ash.ShortcutCustomization.ModifyType.",
                    GetAcceleratorActionName(
                        static_cast<AcceleratorAction>(action_id))}),
      ModificationType::kReset);
  LogRestoreDefault(action_id, result_data->result);
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::RestoreAllDefaults(
    RestoreAllDefaultsCallback callback) {
  CHECK(Shell::Get()->accelerator_prefs()->IsCustomizationAllowed());
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();
  AcceleratorConfigResult result =
      ash_accelerator_configuration_->RestoreAllDefaults();
  result_data->result = result;
  VLOG(1) << "RestoreAllDefaults completed with error code: "
          << result_data->result;
  base::UmaHistogramEnumeration(kShortcutCustomizationHistogramName,
                                ShortcutCustomizationAction::kResetAll);
  std::move(callback).Run(std::move(result_data));
}

void AcceleratorConfigurationProvider::RecordUserAction(
    UserAction user_action) {
  switch (user_action) {
    case UserAction::kOpenEditDialog:
      base::RecordAction(base::UserMetricsAction(
          "ShortcutCustomization_OpenEditAcceleratorDialog"));
      break;
    case UserAction::kStartAddAccelerator:
      base::RecordAction(
          base::UserMetricsAction("ShortcutCustomization_StartAddAccelerator"));
      break;
    case UserAction::kStartReplaceAccelerator:
      base::RecordAction(base::UserMetricsAction(
          "ShortcutCustomization_StartReplaceAccelerator"));
      break;
    case UserAction::kRemoveAccelerator:
      base::RecordAction(
          base::UserMetricsAction("ShortcutCustomization_RemoveAccelerator"));
      break;
    case UserAction::kSuccessfulModification:
      base::RecordAction(base::UserMetricsAction(
          "ShortcutCustomization_SuccessfullyModified"));
      break;
    case UserAction::kResetAction:
      base::RecordAction(
          base::UserMetricsAction("ShortcutCustomization_ResetAction"));
      break;
    case UserAction::kResetAll:
      base::RecordAction(
          base::UserMetricsAction("ShortcutCustomization_ResetAll"));
      break;
  }
}

void AcceleratorConfigurationProvider::RecordMainCategoryNavigation(
    mojom::AcceleratorCategory category) {
  base::UmaHistogramEnumeration(
      "Ash.ShortcutCustomization.MainCategoryNavigation", category);
}

void AcceleratorConfigurationProvider::RecordEditDialogCompletedActions(
    shortcut_customization::mojom::EditDialogCompletedActions
        completed_actions) {
  base::UmaHistogramEnumeration(
      "Ash.ShortcutCustomization.EditDialogCompletedActions",
      completed_actions);
}

void AcceleratorConfigurationProvider::RecordAddOrEditSubactions(
    bool is_add,
    shortcut_customization::mojom::Subactions subactions) {
  const std::string histogram_name =
      is_add ? "Ash.ShortcutCustomization.AddAcceleratorSubactions"
             : "Ash.ShortcutCustomization.EditAcceleratorSubactions";
  base::UmaHistogramEnumeration(histogram_name, subactions);
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
  ScheduleNotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::InitializeNonConfigurableAccelerators(
    NonConfigurableActionsMap mapping) {
  non_configurable_actions_mapping_ = std::move(mapping);
  for (const auto& [ambient_action_id, accelerators_details] :
       non_configurable_actions_mapping_) {
    if (accelerators_details.IsStandardAccelerator()) {
      DCHECK(!accelerators_details.replacements.has_value());
      DCHECK(!accelerators_details.message_id.has_value());
      const AcceleratorLayoutDetails& layout =
          accelerator_layout_lookup_[GetUuid(mojom::AcceleratorSource::kAmbient,
                                             ambient_action_id)];
      for (const auto& accelerator :
           accelerators_details.accelerators.value()) {
        const uint32_t action_id = static_cast<uint32_t>(ambient_action_id);
        // Store accessibility lookups separately.
        if (layout.category == mojom::AcceleratorCategory::kAccessibility) {
          accessibility_accelerator_to_id_.InsertNew(
              std::make_pair(accelerator, action_id));
        } else {
          auto* action_ids =
              non_configurable_accelerator_to_id_.Find(accelerator);
          if (!action_ids) {
            non_configurable_accelerator_to_id_.InsertNew(std::make_pair(
                accelerator, std::vector<AcceleratorActionId>{action_id}));
          } else {
            action_ids->push_back(action_id);
          }
        }
        id_to_non_configurable_accelerators_[action_id].push_back(accelerator);
      }
    }
  }
  ScheduleNotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::OnAcceleratorsUpdated(
    mojom::AcceleratorSource source,
    const ActionIdToAcceleratorsMap& mapping) {
  accelerators_mapping_[source] = mapping;
  ScheduleNotifyAcceleratorsUpdated();
}

void AcceleratorConfigurationProvider::ScheduleNotifyAcceleratorsUpdated() {
  if (!pending_notify_accelerators_updated_) {
    pending_notify_accelerators_updated_ = true;
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AcceleratorConfigurationProvider::NotifyAcceleratorsUpdated,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AcceleratorConfigurationProvider::NotifyAcceleratorsUpdated() {
  pending_notify_accelerators_updated_ = false;

  AcceleratorConfigurationMap config_map = CreateConfigurationMap();
  if (accelerators_updated_mojo_observer_.is_bound()) {
    accelerators_updated_mojo_observer_->OnAcceleratorsUpdated(
        mojo::Clone(config_map));
  }
  for (auto& observer : accelerators_updated_observers_) {
    observer.OnAcceleratorsUpdated(mojo::Clone(config_map));
  }

  // Store a cached copy of the configuration map.
  cached_configuration_ = mojo::Clone(config_map);
}

void AcceleratorConfigurationProvider::CreateAndAppendAliasedAccelerators(
    const ui::Accelerator& accelerator,
    bool locked,
    mojom::AcceleratorType type,
    mojom::AcceleratorState state,
    std::vector<mojom::AcceleratorInfoPtr>& output,
    bool is_accelerator_locked) {
  // Get the alias accelerators by doing F-Keys remapping and
  // (reversed) six-pack-keys remapping if applicable.
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  output.reserve(output.size() + accelerator_aliases.size());

  // Return early if there are no alias accelerators (Because certain keys are
  // unavailable), accelerator will be suppressed/disabled and its state will be
  // `kDisabledByUnavailableKeys`.
  if (accelerator_aliases.empty()) {
    output.push_back(CreateStandardAcceleratorInfo(
        accelerator, is_accelerator_locked, locked,
        GetAcceleratorType(accelerator),
        mojom::AcceleratorState::kDisabledByUnavailableKeys));
    return;
  }

  for (const auto& accelerator_alias : accelerator_aliases) {
    // If `accelerator_alias` is not the original accelerator add the original
    // accelerator to the `AcceleratorInfo`. This allows the frontend to detect
    // what is the real accelerator to configure.
    if (accelerator_alias != accelerator) {
      output.push_back(CreateStandardAcceleratorInfo(
          accelerator_alias, is_accelerator_locked, locked,
          GetAcceleratorType(accelerator), state, accelerator));
    } else {
      output.push_back(CreateStandardAcceleratorInfo(
          accelerator_alias, is_accelerator_locked, locked,
          GetAcceleratorType(accelerator), state));
    }
  }
}

std::optional<AcceleratorResultDataPtr>
AcceleratorConfigurationProvider::PreprocessAddAccelerator(
    mojom::AcceleratorSource source,
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  AcceleratorResultDataPtr result_data = AcceleratorResultData::New();

  // Check if the accelerator is reserved. If so return an error.
  const std::optional<std::u16string> reserved_accelerator_name =
      GetReservedAcceleratorName(accelerator);
  if (reserved_accelerator_name.has_value()) {
    pending_accelerator_.reset();
    result_data->result = AcceleratorConfigResult::kConflict;
    result_data->shortcut_name = std::move(reserved_accelerator_name.value());
    return result_data;
  }

  // Check if `accelerator` conflicts with non-configurable accelerators.
  // This includes: browser, accessbility, and ambient accelerators.
  const std::vector<uint32_t> non_configurable_conflict_ids =
      FindNonConfigurableIdFromAccelerator(accelerator);
  // If there was a conflict with a non-configurable accelerator, return
  // just one of the conflict id's.
  if (!non_configurable_conflict_ids.empty()) {
    pending_accelerator_.reset();
    result_data->result = AcceleratorConfigResult::kConflict;
    // Get the shortcut name and add it to the return struct.
    result_data->shortcut_name = l10n_util::GetStringUTF16(
        accelerator_layout_lookup_[GetUuid(
                                       mojom::AcceleratorSource::kAmbient,
                                       non_configurable_conflict_ids.front())]
            .description_string_id);
    return result_data;
  }

  // Check if the accelerator conflicts with an existing ash accelerator.
  const AcceleratorAction* found_ash_action =
      ash_accelerator_configuration_->FindAcceleratorAction(accelerator);

  // Accelerator does not exist, can add this accelerator.
  if (!found_ash_action) {
    return std::nullopt;
  }

  // Always allow using deprecated accelerators.
  if (ash_accelerator_configuration_->IsDeprecated(accelerator)) {
    return std::nullopt;
  }

  // Check that the new accelerator is not already an existing accelerator of
  // the same action. If so, return with `kConflict`.
  if (*found_ash_action == action_id) {
    pending_accelerator_.reset();
    result_data->result = AcceleratorConfigResult::kConflict;
    result_data->shortcut_name = l10n_util::GetStringUTF16(
        accelerator_layout_lookup_[GetUuid(mojom::AcceleratorSource::kAsh,
                                           *found_ash_action)]
            .description_string_id);
    return result_data;
  }

  const auto& layout_iter = accelerator_layout_lookup_.find(
      GetUuid(mojom::AcceleratorSource::kAsh, *found_ash_action));
  // If there is a valid `found_ash_action` with no layout lookup associated
  // with it, this indicates a hidden accelerator not displayed in the
  // shortcuts app. Allow this accelerator to be used for the new action.
  if (layout_iter == accelerator_layout_lookup_.end()) {
    return std::nullopt;
  }

  // The accelerator is not hidden and appears in the app, go through conflict
  // detection checks.

  // Accelerator already exists, check if it belongs to a locked action.
  const AcceleratorLayoutDetails& layout_details = layout_iter->second;
  const std::u16string& shortcut_name =
      l10n_util::GetStringUTF16(layout_details.description_string_id);
  if (layout_details.locked) {
    pending_accelerator_.reset();
    result_data->result = AcceleratorConfigResult::kActionLocked;
    result_data->shortcut_name = shortcut_name;
    return result_data;
  }

  // If not locked, then check if the user has already pressed the accelerator
  // for this action. If not, store it and return the error. If this is a
  // different accelerator then store it.
  if (!pending_accelerator_ || pending_accelerator_->action != action_id ||
      pending_accelerator_->source != source ||
      pending_accelerator_->accelerator != accelerator) {
    result_data->result = mojom::AcceleratorConfigResult::kConflictCanOverride;
    pending_accelerator_.reset();
    pending_accelerator_ =
        std::make_unique<PendingAccelerator>(accelerator, source, action_id);
    result_data->shortcut_name = shortcut_name;
    conflict_error_state_ =
        AcceleratorConflictErrorState::kAwaitingConflictResolution;
    return result_data;
  }

  if (pending_accelerator_->accelerator == accelerator &&
      conflict_error_state_ ==
          AcceleratorConflictErrorState::kAwaitingConflictResolution) {
    conflict_error_state_ = AcceleratorConflictErrorState::kConflictResolved;
  }
  return std::nullopt;
}

AcceleratorConfigurationProvider::AcceleratorConflictErrorState
AcceleratorConfigurationProvider::MaybeHandleNonSearchAccelerator(
    const ui::Accelerator& accelerator,
    mojom::AcceleratorSource source,
    AcceleratorActionId action_id) {
  // Disable non-search accelerator warning when re-adding the default
  // accelerator.
  if (base::Contains(GetDefaultAcceleratorsForId(action_id), accelerator)) {
    return AcceleratorConflictErrorState::kStandby;
  }

  if (conflict_error_state_ !=
      AcceleratorConflictErrorState::kAwaitingNonSearchConfirmation) {
    pending_accelerator_.reset();
  }

  // Function keys cannot be used with the search modifier so don't warn users.
  if ((accelerator.modifiers() & ui::EF_COMMAND_DOWN) == 0 &&
      !ui::KeyboardCapability::IsFunctionKey(accelerator.key_code())) {
    if (!pending_accelerator_ || pending_accelerator_->action != action_id ||
        pending_accelerator_->source != source ||
        pending_accelerator_->accelerator != accelerator) {
      pending_accelerator_ =
          std::make_unique<PendingAccelerator>(accelerator, source, action_id);
      return AcceleratorConflictErrorState::kAwaitingNonSearchConfirmation;
    }
  }
  return AcceleratorConflictErrorState::kStandby;
}

std::vector<uint32_t>
AcceleratorConfigurationProvider::FindNonConfigurableIdFromAccelerator(
    const ui::Accelerator& accelerator) {
  std::vector<uint32_t> ids;
  // Check browser/text non-configurable accelerators first.
  auto* non_configurable_conflict_ids =
      non_configurable_accelerator_to_id_.Find(accelerator);

  if (non_configurable_conflict_ids) {
    for (const auto id : *non_configurable_conflict_ids) {
      ids.push_back(id);
    }
  }

  // Then check accessibility accelerators.
  uint32_t* non_configurable_conflict_id =
      accessibility_accelerator_to_id_.Find(accelerator);

  if (non_configurable_conflict_id) {
    ids.push_back(*non_configurable_conflict_id);
  }

  return ids;
}

void AcceleratorConfigurationProvider::SetLayoutDetailsMapForTesting(
    const std::vector<AcceleratorLayoutDetails>& layouts) {
  accelerator_layout_lookup_.clear();
  for (const auto& layout : layouts) {
    accelerator_layout_lookup_[GetUuid(layout.source, layout.action_id)] =
        layout;
  }
}

std::vector<ui::Accelerator>
AcceleratorConfigurationProvider::GetDefaultAcceleratorsForId(
    uint32_t action_id) const {
  const std::vector<ui::Accelerator>& raw_default_accelerators =
      ash_accelerator_configuration_->GetDefaultAcceleratorsForId(action_id);

  std::vector<ui::Accelerator> default_accelerators;
  for (const auto& accelerator : raw_default_accelerators) {
    // Filter the hidden accelerators.
    if (IsAcceleratorHidden(action_id, accelerator)) {
      continue;
    }
    // Get the alias accelerators.
    std::vector<ui::Accelerator> accelerator_aliases =
        accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
    // Return early if there are no alias accelerators. This will filter the
    // disabled accelerators due to unavailable keys.
    if (accelerator_aliases.empty()) {
      continue;
    }
    for (const auto& accelerator_alias : accelerator_aliases) {
      default_accelerators.push_back(accelerator_alias);
    }
  }
  return default_accelerators;
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

  for (const auto& layout_id : kAcceleratorLayouts) {
    const std::optional<AcceleratorLayoutDetails> layout =
        GetAcceleratorLayout(layout_id);
    if (!layout) {
      LOG(ERROR) << "Failed to get Accelerator layout for id: " << layout_id;
      continue;
    }
    if (layout->source != mojom::AcceleratorSource::kAsh) {
      // Only ash accelerators can have dynamically modified properties.
      // Note that ambient accelerators cannot be in kAsh.
      continue;
    }

    // Remove layouts were initially added but should now be removed if they
    // are in the block list.
    if (ShouldExcludeItem(*layout)) {
      const std::string uuid = GetUuid(layout->source, layout->action_id);
      if (accelerator_layout_lookup_.contains(uuid)) {
        accelerator_layout_lookup_.erase(uuid);
        std::erase_if(layout_infos_,
                      [&](const mojom::AcceleratorLayoutInfoPtr& info) {
                        return info->source == layout->source &&
                               info->action == layout->action_id;
                      });
      }
      continue;
    }

    const auto& id_to_accelerator_iter =
        id_to_accelerators.find(layout->action_id);
    // For tests, we only want to test a subset of accelerators so it's possible
    // that we don't have accelerators for the given `layout_info`.
    if (id_to_accelerator_iter == id_to_accelerators.end() &&
        ignore_layouts_for_testing_) {
      continue;
    }

    // TODO(jimmyxgong): Re-evaluate this after fixing the root cause of this.
    if (id_to_accelerator_iter == id_to_accelerators.end()) {
      LOG(ERROR) << "Error: Layout with action ID: " << layout->action_id
                 << " does not exist in the ID to Accelerator mapping. "
                 << " Skipping adding this to the Ash configuration map.";
      continue;
    }

    const auto& accelerators = id_to_accelerator_iter->second;

    // Check if the default accelerators are available, if not re-add them but
    // mark them as disabled.
    const std::vector<ui::Accelerator>& default_accelerators =
        ash_accelerator_configuration_->GetDefaultAcceleratorsForId(
            layout->action_id);
    for (const auto& default_accelerator : default_accelerators) {
      if (base::Contains(accelerators, default_accelerator)) {
        continue;
      }
      const bool is_accelerator_locked =
          ash_accelerator_configuration_->IsAcceleratorLocked(
              default_accelerator);

      // Append the missing default accelerators but marked as disabled by user.
      CreateAndAppendAliasedAccelerators(
          default_accelerator, layout->locked, mojom::AcceleratorType::kDefault,
          mojom::AcceleratorState::kDisabledByUser,
          output_action_id_to_accelerators[layout->action_id],
          is_accelerator_locked);
    }

    for (const auto& accelerator : accelerators) {
      if (IsAcceleratorHidden(layout->action_id, accelerator)) {
        continue;
      }
      const bool is_accelerator_locked =
          ash_accelerator_configuration_->IsAcceleratorLocked(accelerator);
      CreateAndAppendAliasedAccelerators(
          accelerator, layout->locked, mojom::AcceleratorType::kDefault,
          mojom::AcceleratorState::kEnabled,
          output_action_id_to_accelerators[layout->action_id],
          is_accelerator_locked);
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
