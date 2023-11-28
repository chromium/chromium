// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"

#include <cstdint>

#include "ash/accelerators/accelerator_encoding.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/keyboard_mouse_combo_device_metrics.h"

namespace ash {

namespace {

enum class PointerSensitivity {
  kLowest = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
  kHighest = 5,
  kMaxValue = kHighest,
};

// Do not change ordering of this list as the ordering is used to compute
// modifier hash in `RecordModifierRemappingHash()`.
static constexpr struct {
  const char* key_name;
  ui::mojom::ModifierKey modifier_key;
} kModifierNames[] = {
    {"Meta", ui::mojom::ModifierKey::kMeta},
    {"Control", ui::mojom::ModifierKey::kControl},
    {"Alt", ui::mojom::ModifierKey::kAlt},
    {"CapsLock", ui::mojom::ModifierKey::kCapsLock},
    {"Escape", ui::mojom::ModifierKey::kEscape},
    {"Backspace", ui::mojom::ModifierKey::kBackspace},
    {"Assistant", ui::mojom::ModifierKey::kAssistant},
};

// The modifier hash is made up of `kNumModifiers` blocks of
// `kModifierHashWidth` bits. Each modifier is assigned a `kModifierHashWidth`
// width block to track its user configured setting. These user configured
// settings are contained within [0, `kMaxModifierValue`] and are assigned in
// /ash/public/input_device_settings.mojom in the `mojom::ModifierKey` struct.

// To decode, break up the hash into `kModifierHashWidth` bit integers.
// For example, if `kModifierHashWidth` is 4, use the following bit ranges to
// extract the value of the remapped modifier:

// | index | ModifierKey             | Bit Range |
// | 0     | kMeta                   | [0, 3]    |
// | 1     | kControl                | [4, 7]    |
// | 2     | kAlt                    | [8, 11]   |
// | 3     | kCapsLock               | [12, 15]  |
// | 4     | kEscape                 | [16, 19]  |
// | 5     | kBackspace              | [20, 23]  |
// | 6     | kAssistant              | [24, 27]  |

// Each modifier key will have 9 actions which requires 4 bits to encode.
constexpr int kModifierHashWidth = 4;
constexpr int kMaxModifierValue = (1 << kModifierHashWidth) - 1;
constexpr int kNumModifiers = std::size(kModifierNames);

// Verify that the number of modifiers we are trying to hash together into a
// 32-bit int will fit without any overflow or UB.
// Modifier hash is limited to 32 bits as metrics can only handle 32 bit ints.
static_assert((sizeof(int32_t) * 8) >= (kModifierHashWidth * kNumModifiers));
static_assert(static_cast<int>(ui::mojom::ModifierKey::kMaxValue) <=
              kMaxModifierValue);

// Precomputes the value of the modifier hash when all prefs are configured to
// their default value.
constexpr int32_t PrecalculateDefaultModifierHash() {
  uint32_t hash = 0;
  for (ssize_t i = kNumModifiers - 1u; i >= 0; i--) {
    hash <<= kModifierHashWidth;
    hash += static_cast<int>(kModifierNames[i].modifier_key);
  }
  return hash;
}
constexpr int32_t kDefaultModifierHash = PrecalculateDefaultModifierHash();

std::string GetKeyboardMetricsPrefix(const mojom::Keyboard& keyboard) {
  if (!keyboard.is_external) {
    return "ChromeOS.Settings.Device.Keyboard.Internal.";
  } else if (keyboard.meta_key == mojom::MetaKey::kLauncher ||
             keyboard.meta_key == mojom::MetaKey::kSearch) {
    return "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.";
  } else {
    return "ChromeOS.Settings.Device.Keyboard.External.";
  }
}

ui::mojom::ModifierKey GetModifierRemappingTo(
    const mojom::KeyboardSettings& settings,
    ui::mojom::ModifierKey modifier_key) {
  const auto iter = settings.modifier_remappings.find(modifier_key);
  if (iter != settings.modifier_remappings.end()) {
    return iter->second;
  }
  return modifier_key;
}

std::optional<std::string> GetModifierKeyName(
    ui::mojom::ModifierKey modifier_key) {
  for (ssize_t i = kNumModifiers - 1; i >= 0; i--) {
    if (kModifierNames[i].modifier_key == modifier_key) {
      return std::make_optional<std::string>(kModifierNames[i].key_name);
    }
  }
  return std::nullopt;
}

int GetNumberOfNonDefaultRemappings(
    const mojom::KeyboardSettings& settings,
    const base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey>&
        default_remappings) {
  int num_keys_changed = 0;
  const auto& remappings = settings.modifier_remappings;

  // Count the number of different pairs in the current remapping that is not
  // default. For defaults on apple keyboard, it would be a flat_map:
  // {{ModifierKey.Meta => ModifierKey.Control},
  //  {ModifierKey.Control => ModifierKey.Meta}},
  // For others, it's an empty flat_map.
  // A remapping pair [remapped_from, remapped_to] in current remapping is
  // considered to be non-default if:
  //   1. remapped_from does not exist in the default remapping.
  //   2. remapped_from is mapped to any key other than its default mapping.
  for (const auto& [remapped_from, remapped_to] : remappings) {
    const auto& iter = default_remappings.find(remapped_from);
    if (iter == default_remappings.end() || iter->second != remapped_to) {
      ++num_keys_changed;
    }
  }
  for (const auto& [remapped_from, remapped_to] : default_remappings) {
    if (!remappings.contains(remapped_from)) {
      ++num_keys_changed;
    }
  }
  return num_keys_changed;
}

ui::mojom::SixPackShortcutModifier GetSixPackKeyModifier(
    const mojom::Keyboard& keyboard,
    ui::KeyboardCode key_code) {
  CHECK(ui::KeyboardCapability::IsSixPackKey(key_code));
  CHECK(keyboard.settings->six_pack_key_remappings);
  switch (key_code) {
    case ui::VKEY_DELETE:
      return keyboard.settings->six_pack_key_remappings->del;
    case ui::VKEY_INSERT:
      return keyboard.settings->six_pack_key_remappings->insert;
    case ui::VKEY_HOME:
      return keyboard.settings->six_pack_key_remappings->home;
    case ui::VKEY_END:
      return keyboard.settings->six_pack_key_remappings->end;
    case ui::VKEY_PRIOR:
      return keyboard.settings->six_pack_key_remappings->page_up;
    case ui::VKEY_NEXT:
      return keyboard.settings->six_pack_key_remappings->page_down;
    default:
      NOTREACHED_NORETURN();
  }
}
std::string GetSixPackKeyMetricName(const std::string& prefix,
                                    ui::KeyboardCode key_code,
                                    bool is_initial_value) {
  CHECK(ui::KeyboardCapability::IsSixPackKey(key_code));
  std::string key_name;
  switch (key_code) {
    case ui::VKEY_DELETE:
      key_name = "Delete";
      break;
    case ui::VKEY_INSERT:
      key_name = "Insert";
      break;
    case ui::VKEY_HOME:
      key_name = "Home";
      break;
    case ui::VKEY_END:
      key_name = "End";
      break;
    case ui::VKEY_PRIOR:
      key_name = "PageUp";
      break;
    case ui::VKEY_NEXT:
      key_name = "PageDown";
      break;
    default:
      NOTREACHED_NORETURN();
  }
  return base::StrCat({prefix, "SixPackKeys.", key_name,
                       is_initial_value ? ".Initial" : ".Changed"});
}

void RecordKeyboardNumberOfKeysRemapped(const mojom::Keyboard& keyboard) {
  base::flat_map<ui::mojom::ModifierKey, ui::mojom::ModifierKey>
      default_remappings;
  if (keyboard.meta_key == mojom::MetaKey::kCommand) {
    default_remappings[ui::mojom::ModifierKey::kControl] =
        ui::mojom::ModifierKey::kMeta;
    default_remappings[ui::mojom::ModifierKey::kMeta] =
        ui::mojom::ModifierKey::kControl;
  }
  const int num_keys_remapped = GetNumberOfNonDefaultRemappings(
      *keyboard.settings, std::move(default_remappings));
  const std::string keyboard_metrics =
      base::StrCat({GetKeyboardMetricsPrefix(keyboard),
                    "Modifiers.NumberOfRemappedKeysOnStart"});
  base::UmaHistogramCounts100(keyboard_metrics, num_keys_remapped);
}

bool ShouldRecordFkeyMetrics(const mojom::Keyboard& keyboard) {
  return ::features::AreF11AndF12ShortcutsEnabled() &&
         Shell::Get()->keyboard_capability()->IsChromeOSKeyboard(keyboard.id) &&
         (keyboard.settings->f11.has_value() &&
          keyboard.settings->f12.has_value());
}

void RecordButtonRemappingNameIfChanged(
    const mojom::ButtonRemappingPtr& original_remapping,
    const mojom::ButtonRemappingPtr& new_remapping,
    const char* peripheral_kind) {
  const std::string metric_name_prefix =
      base::StrCat({"ChromeOS.Settings.Device.", peripheral_kind,
                    ".ButtonRemapping.Name.Changed."});
  if (original_remapping->name != new_remapping->name) {
    if (original_remapping->button->is_customizable_button()) {
      base::UmaHistogramEnumeration(
          base::StrCat({metric_name_prefix, "CustomizableButton"}),
          original_remapping->button->get_customizable_button());
    } else if (original_remapping->button->is_vkey()) {
      base::UmaHistogramSparse(base::StrCat({metric_name_prefix, "Vkey"}),
                               original_remapping->button->get_vkey());
    }
  }
}

template <typename T>
base::StringPiece GetDeviceTypeMetricsName() {
  if constexpr (std::is_same_v<T, mojom::Keyboard>) {
    return "Keyboard";
  } else if constexpr (std::is_same_v<T, mojom::Mouse>) {
    return "Mouse";
  } else if constexpr (std::is_same_v<T, mojom::Touchpad>) {
    return "Touchpad";
  } else if constexpr (std::is_same_v<T, mojom::PointingStick>) {
    return "PointingStick";
  }
}

template <typename T>
base::StringPiece GetSettingsUpdatedPrefName() {
  if constexpr (std::is_same_v<T, mojom::Keyboard>) {
    return prefs::kKeyboardUpdateSettingsMetricInfo;
  } else if constexpr (std::is_same_v<T, mojom::Mouse>) {
    return prefs::kMouseUpdateSettingsMetricInfo;
  } else if constexpr (std::is_same_v<T, mojom::Touchpad>) {
    return prefs::kTouchpadUpdateSettingsMetricInfo;
  } else if constexpr (std::is_same_v<T, mojom::PointingStick>) {
    return prefs::kPointingStickUpdateSettingsMetricInfo;
  }
}

base::StringPiece GetSettingsUpdatedTimePeriodMetricName(
    SettingsUpdatedMetricsInfo::TimePeriod time_period) {
  constexpr auto kTimePeriodToMetricName =
      base::MakeFixedFlatMap<SettingsUpdatedMetricsInfo::TimePeriod,
                             base::StringPiece>({
          {SettingsUpdatedMetricsInfo::TimePeriod::kOneHour, "OneHour"},
          {SettingsUpdatedMetricsInfo::TimePeriod::kThreeHours, "ThreeHours"},
          {SettingsUpdatedMetricsInfo::TimePeriod::kOneDay, "OneDay"},
          {SettingsUpdatedMetricsInfo::TimePeriod::kThreeDays, "ThreeDays"},
          {SettingsUpdatedMetricsInfo::TimePeriod::kOneWeek, "OneWeek"},
      });
  return kTimePeriodToMetricName.at(time_period);
}

base::StringPiece GetSettingsUpdatedCategoryName(
    SettingsUpdatedMetricsInfo::Category category) {
  constexpr auto kCategoryToMetricName =
      base::MakeFixedFlatMap<SettingsUpdatedMetricsInfo::Category,
                             base::StringPiece>({
          {SettingsUpdatedMetricsInfo::Category::kFirstEver, "FirstEver"},
          {SettingsUpdatedMetricsInfo::Category::kDefault, "FromDefaults"},
          {SettingsUpdatedMetricsInfo::Category::kSynced, "Synced"},
      });
  return kCategoryToMetricName.at(category);
}

template <typename T>
void RecordSettingsUpdatedMetric(
    const T& device,
    const SettingsUpdatedMetricsInfo& metrics_info,
    SettingsUpdatedMetricsInfo::TimePeriod time_period_to_record) {
  const std::string metric_name = base::StrCat(
      {"ChromeOS.Settings.Device.", GetDeviceTypeMetricsName<T>(),
       ".SettingsUpdated.",
       GetSettingsUpdatedCategoryName(metrics_info.category()), ".",
       GetSettingsUpdatedTimePeriodMetricName(time_period_to_record)});
  base::UmaHistogramCounts100(metric_name,
                              metrics_info.GetCount(time_period_to_record));
}

template <typename T>
void HandleSettingsUpdatedMetric(const T& device) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(pref_service);

  const auto& settings_update_info_dict =
      pref_service->GetDict(GetSettingsUpdatedPrefName<T>());
  const auto* device_settings_update_info_dict =
      settings_update_info_dict.FindDict(device.device_key);
  if (!device_settings_update_info_dict) {
    return;
  }

  std::optional<SettingsUpdatedMetricsInfo> metrics_info_optional =
      SettingsUpdatedMetricsInfo::FromDict(*device_settings_update_info_dict);
  if (!metrics_info_optional) {
    return;
  }

  SettingsUpdatedMetricsInfo& metrics_info = *metrics_info_optional;
  auto time_period = metrics_info.RecordSettingsUpdate(base::Time::Now());
  if (time_period) {
    RecordSettingsUpdatedMetric(device, metrics_info, *time_period);
  }

  auto updated_settings_update_info_dict = settings_update_info_dict.Clone();
  updated_settings_update_info_dict.Set(device.device_key,
                                        metrics_info.ToDict());
  pref_service->SetDict(std::string(GetSettingsUpdatedPrefName<T>()),
                        std::move(updated_settings_update_info_dict));
}

void RecordButtonRemappingAction(
    const mojom::ButtonRemappingPtr& button_remapping,
    const char* peripheral_kind,
    const char* metric_name_suffix) {
  if (!button_remapping->remapping_action) {
    // TOOD(dpad): Add metric for recording default button remapping.
    return;
  }

  const std::string metric_name_prefix = base::StrCat(
      {"ChromeOS.Settings.Device.", peripheral_kind, ".ButtonRemapping."});
  switch (button_remapping->remapping_action->which()) {
    case mojom::RemappingAction::Tag::kAcceleratorAction:
      base::UmaHistogramSparse(
          base::StrCat(
              {metric_name_prefix, "AcceleratorAction.", metric_name_suffix}),
          button_remapping->remapping_action->get_accelerator_action());
      break;
    case mojom::RemappingAction::Tag::kStaticShortcutAction:
      base::UmaHistogramEnumeration(
          base::StrCat({metric_name_prefix, "StaticShortcutAction.",
                        metric_name_suffix}),
          button_remapping->remapping_action->get_static_shortcut_action());
      break;
    case mojom::RemappingAction::Tag::kKeyEvent:
      base::UmaHistogramSparse(
          base::StrCat({metric_name_prefix, "KeyEvent.", metric_name_suffix}),
          GetEncodedShortcut(
              button_remapping->remapping_action->get_key_event()->modifiers,
              button_remapping->remapping_action->get_key_event()->vkey));
      break;
  }
}

void RecordButtonRemappingActionIfChanged(
    const mojom::ButtonRemappingPtr& original_remapping,
    const mojom::ButtonRemappingPtr& new_remapping,
    const char* peripheral_kind) {
  if (original_remapping->remapping_action != new_remapping->remapping_action) {
    RecordButtonRemappingAction(new_remapping, peripheral_kind, "Changed");
  }
}

void RecordCurrentButtonRemappingAction(
    const mojom::ButtonRemappingPtr& button_remapping,
    const char* peripheral_kind) {
  RecordButtonRemappingAction(button_remapping, peripheral_kind, "Initial");
}

}  // namespace

InputDeviceSettingsMetricsManager::InputDeviceSettingsMetricsManager() =
    default;
InputDeviceSettingsMetricsManager::~InputDeviceSettingsMetricsManager() =
    default;

void InputDeviceSettingsMetricsManager::RecordKeyboardInitialMetrics(
    const mojom::Keyboard& keyboard) {
  // Only record the metrics once for each keyboard.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_keyboards_.find(account_id);

  if (iter != recorded_keyboards_.end() &&
      base::Contains(iter->second, keyboard.device_key)) {
    return;
  }
  recorded_keyboards_[account_id].insert(keyboard.device_key);

  const std::string keyboard_metrics_prefix =
      GetKeyboardMetricsPrefix(keyboard);

  base::UmaHistogramBoolean(keyboard_metrics_prefix + "TopRowAreFKeys.Initial",
                            keyboard.settings->top_row_are_fkeys);
  // Only record BlockMetaFKeyRewrites when keyboard is external/external
  // chromeos.
  if (keyboard.is_external) {
    base::UmaHistogramBoolean(
        keyboard_metrics_prefix + "BlockMetaFKeyRewrites.Initial",
        keyboard.settings->suppress_meta_fkey_rewrites);
  }

  // Record metrics for modifier remappings.
  for (const auto modifier_key : keyboard.modifier_keys) {
    const auto modifier_name = GetModifierKeyName(modifier_key);
    CHECK(modifier_name.has_value());
    const auto key_remapped_to =
        GetModifierRemappingTo(*keyboard.settings, modifier_key);
    const std::string modifier_remapping_metrics =
        base::StrCat({keyboard_metrics_prefix, "Modifiers.", *modifier_name,
                      "RemappedTo.Initial"});
    base::UmaHistogramEnumeration(modifier_remapping_metrics, key_remapped_to);
  }

  // Record remapping metrics when keyboard is initialized.
  RecordModifierRemappingHash(keyboard);
  RecordKeyboardNumberOfKeysRemapped(keyboard);
  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    RecordSixPackKeyInfo(keyboard, ui::VKEY_DELETE, /*is_initial_value=*/true);
    RecordSixPackKeyInfo(keyboard, ui::VKEY_INSERT, /*is_initial_value=*/true);
    RecordSixPackKeyInfo(keyboard, ui::VKEY_HOME, /*is_initial_value=*/true);
    RecordSixPackKeyInfo(keyboard, ui::VKEY_END, /*is_initial_value=*/true);
    RecordSixPackKeyInfo(keyboard, ui::VKEY_PRIOR, /*is_initial_value=*/true);
    RecordSixPackKeyInfo(keyboard, ui::VKEY_NEXT, /*is_initial_value=*/true);
  }

  if (ShouldRecordFkeyMetrics(keyboard)) {
    base::UmaHistogramEnumeration(keyboard_metrics_prefix + "F11.Initial",
                                  keyboard.settings->f11.value());
    base::UmaHistogramEnumeration(keyboard_metrics_prefix + "F12.Initial",
                                  keyboard.settings->f12.value());
  }
}

void InputDeviceSettingsMetricsManager::RecordKeyboardChangedMetrics(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardSettings& old_settings) {
  const std::string keyboard_metrics_prefix =
      GetKeyboardMetricsPrefix(keyboard);

  if (keyboard.settings->top_row_are_fkeys != old_settings.top_row_are_fkeys) {
    base::UmaHistogramBoolean(
        keyboard_metrics_prefix + "TopRowAreFKeys.Changed",
        keyboard.settings->top_row_are_fkeys);
  }
  // Only record BlockMetaFKeyRewrites when keyboard is external/external
  // chromeos.
  if (keyboard.is_external && keyboard.settings->suppress_meta_fkey_rewrites !=
                                  old_settings.suppress_meta_fkey_rewrites) {
    base::UmaHistogramBoolean(
        keyboard_metrics_prefix + "BlockMetaFKeyRewrites.Changed",
        keyboard.settings->suppress_meta_fkey_rewrites);
  }

  // Record metrics for modifier remappings.
  for (const auto modifier_key : keyboard.modifier_keys) {
    const auto modifier_name = GetModifierKeyName(modifier_key);
    CHECK(modifier_name.has_value());
    const auto key_remapped_to_before =
        GetModifierRemappingTo(old_settings, modifier_key);
    const auto key_remapped_to =
        GetModifierRemappingTo(*keyboard.settings, modifier_key);
    // Only emit the metric if the modifier remapping is changed.
    if (key_remapped_to_before != key_remapped_to) {
      const std::string modifier_remapping_metrics =
          base::StrCat({keyboard_metrics_prefix, "Modifiers.", *modifier_name,
                        "RemappedTo.Changed"});
      base::UmaHistogramEnumeration(modifier_remapping_metrics,
                                    key_remapped_to);
    }
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    CHECK(keyboard.settings->six_pack_key_remappings);
    if (keyboard.settings->six_pack_key_remappings->del !=
        old_settings.six_pack_key_remappings->del) {
      RecordSixPackKeyInfo(keyboard, ui::VKEY_DELETE,
                           /*is_initial_value=*/false);
    }
    if (keyboard.settings->six_pack_key_remappings->insert !=
        old_settings.six_pack_key_remappings->insert) {
      RecordSixPackKeyInfo(keyboard, ui::VKEY_INSERT,
                           /*is_initial_value=*/false);
    }
    if (keyboard.settings->six_pack_key_remappings->home !=
        old_settings.six_pack_key_remappings->home) {
      RecordSixPackKeyInfo(keyboard, ui::VKEY_HOME, /*is_initial_value=*/false);
    }
    if (keyboard.settings->six_pack_key_remappings->end !=
        old_settings.six_pack_key_remappings->end) {
      RecordSixPackKeyInfo(keyboard, ui::VKEY_END, /*is_initial_value=*/false);
    }
    if (keyboard.settings->six_pack_key_remappings->page_up !=
        old_settings.six_pack_key_remappings->page_up) {
      RecordSixPackKeyInfo(keyboard, ui::VKEY_PRIOR,
                           /*is_initial_value=*/false);
    }
    if (keyboard.settings->six_pack_key_remappings->page_down !=
        old_settings.six_pack_key_remappings->page_down) {
      RecordSixPackKeyInfo(keyboard, ui::VKEY_NEXT, /*is_initial_value=*/false);
    }
  }

  if (ShouldRecordFkeyMetrics(keyboard)) {
    if (keyboard.settings->f11 != old_settings.f11) {
      base::UmaHistogramEnumeration(keyboard_metrics_prefix + "F11.Changed",
                                    keyboard.settings->f11.value());
    }
    if (keyboard.settings->f12 != old_settings.f12) {
      base::UmaHistogramEnumeration(keyboard_metrics_prefix + "F12.Changed",
                                    keyboard.settings->f12.value());
    }
  }
  HandleSettingsUpdatedMetric(keyboard);
}

void InputDeviceSettingsMetricsManager::RecordKeyboardNumberOfKeysReset(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardSettings& default_settings) {
  const int num_keys_reset = GetNumberOfNonDefaultRemappings(
      *keyboard.settings, default_settings.modifier_remappings);

  if (num_keys_reset != 0) {
    const std::string keyboard_metrics = base::StrCat(
        {GetKeyboardMetricsPrefix(keyboard), "Modifiers.NumberOfKeysReset"});
    base::UmaHistogramCounts100(keyboard_metrics, num_keys_reset);
  }
  HandleSettingsUpdatedMetric(keyboard);
}

void InputDeviceSettingsMetricsManager::RecordMouseInitialMetrics(
    const mojom::Mouse& mouse) {
  // Only record the metrics once for each mouse.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_mice_.find(account_id);

  if (iter != recorded_mice_.end() &&
      base::Contains(iter->second, mouse.device_key)) {
    return;
  }
  recorded_mice_[account_id].insert(mouse.device_key);

  PointerSensitivity sensitivity =
      static_cast<PointerSensitivity>(mouse.settings->sensitivity);
  PointerSensitivity scroll_sensitivity =
      static_cast<PointerSensitivity>(mouse.settings->scroll_sensitivity);

  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.AccelerationEnabled.Initial",
      mouse.settings->acceleration_enabled);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.ScrollAcceleration.Initial",
      mouse.settings->scroll_acceleration);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.ReverseScrolling.Initial",
      mouse.settings->reverse_scrolling);
  base::UmaHistogramEnumeration(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Initial", sensitivity);
  base::UmaHistogramEnumeration(
      "ChromeOS.Settings.Device.Mouse.ScrollSensitivity.Initial",
      scroll_sensitivity);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.SwapPrimaryButtons.Initial",
      mouse.settings->swap_right);

  for (const auto& button_remapping : mouse.settings->button_remappings) {
    RecordCurrentButtonRemappingAction(button_remapping,
                                       /*peripheral_kind=*/"Mouse");
  }
}

void InputDeviceSettingsMetricsManager::RecordMouseChangedMetrics(
    const mojom::Mouse& mouse,
    const mojom::MouseSettings& old_settings) {
  if (mouse.settings->acceleration_enabled !=
      old_settings.acceleration_enabled) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.AccelerationEnabled.Changed",
        mouse.settings->acceleration_enabled);
  }
  if (mouse.settings->scroll_acceleration != old_settings.scroll_acceleration) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.ScrollAcceleration.Changed",
        mouse.settings->scroll_acceleration);
  }
  if (mouse.settings->reverse_scrolling != old_settings.reverse_scrolling) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.ReverseScrolling.Changed",
        mouse.settings->reverse_scrolling);
  }
  if (mouse.settings->sensitivity != old_settings.sensitivity) {
    PointerSensitivity sensitivity =
        static_cast<PointerSensitivity>(mouse.settings->sensitivity);
    base::UmaHistogramEnumeration(
        "ChromeOS.Settings.Device.Mouse.Sensitivity.Changed", sensitivity);
    const int speed_difference =
        mouse.settings->sensitivity - old_settings.sensitivity;
    const std::string delta_sensitivity_metric =
        speed_difference > 0
            ? "ChromeOS.Settings.Device.Mouse.Sensitivity.Increase"
            : "ChromeOS.Settings.Device.Mouse.Sensitivity.Decrease";
    base::UmaHistogramEnumeration(
        delta_sensitivity_metric,
        static_cast<PointerSensitivity>(abs(speed_difference)));
  }
  if (mouse.settings->scroll_sensitivity != old_settings.scroll_sensitivity) {
    PointerSensitivity scroll_sensitivity =
        static_cast<PointerSensitivity>(mouse.settings->scroll_sensitivity);
    base::UmaHistogramEnumeration(
        "ChromeOS.Settings.Device.Mouse.ScrollSensitivity.Changed",
        scroll_sensitivity);
    const int speed_difference =
        mouse.settings->scroll_sensitivity - old_settings.scroll_sensitivity;
    const std::string delta_scroll_sensitivity_metric =
        speed_difference > 0
            ? "ChromeOS.Settings.Device.Mouse.ScrollSensitivity.Increase"
            : "ChromeOS.Settings.Device.Mouse.ScrollSensitivity.Decrease";
    base::UmaHistogramEnumeration(
        delta_scroll_sensitivity_metric,
        static_cast<PointerSensitivity>(abs(speed_difference)));
  }
  if (mouse.settings->swap_right != old_settings.swap_right) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.SwapPrimaryButtons.Changed",
        mouse.settings->swap_right);
  }
  for (const auto& new_remapping : mouse.settings->button_remappings) {
    for (const auto& original_remapping : old_settings.button_remappings) {
      if (original_remapping->button == new_remapping->button) {
        RecordButtonRemappingNameIfChanged(original_remapping, new_remapping,
                                           /*peripheral_kind=*/
                                           "Mouse");
        RecordButtonRemappingActionIfChanged(original_remapping, new_remapping,
                                             /*peripheral_kind=*/
                                             "Mouse");
      }
    }
  }
  HandleSettingsUpdatedMetric(mouse);
}

void InputDeviceSettingsMetricsManager::RecordPointingStickInitialMetrics(
    const mojom::PointingStick& pointing_stick) {
  // Only record the metrics once for each pointing stick.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_pointing_sticks_.find(account_id);

  if (iter != recorded_pointing_sticks_.end() &&
      base::Contains(iter->second, pointing_stick.device_key)) {
    return;
  }
  recorded_pointing_sticks_[account_id].insert(pointing_stick.device_key);

  PointerSensitivity sensitivity =
      static_cast<PointerSensitivity>(pointing_stick.settings->sensitivity);

  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.PointingStick.AccelerationEnabled.Initial",
      pointing_stick.settings->acceleration_enabled);
  base::UmaHistogramEnumeration(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Initial",
      sensitivity);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.PointingStick.SwapPrimaryButtons.Initial",
      pointing_stick.settings->swap_right);
}

void InputDeviceSettingsMetricsManager::RecordPointingStickChangedMetrics(
    const mojom::PointingStick& pointing_stick,
    const mojom::PointingStickSettings& old_settings) {
  if (pointing_stick.settings->acceleration_enabled !=
      old_settings.acceleration_enabled) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.PointingStick.AccelerationEnabled.Changed",
        pointing_stick.settings->acceleration_enabled);
  }
  if (pointing_stick.settings->sensitivity != old_settings.sensitivity) {
    PointerSensitivity sensitivity =
        static_cast<PointerSensitivity>(pointing_stick.settings->sensitivity);
    base::UmaHistogramEnumeration(
        "ChromeOS.Settings.Device.PointingStick.Sensitivity.Changed",
        sensitivity);
    const int speed_difference =
        pointing_stick.settings->sensitivity - old_settings.sensitivity;
    const std::string delta_sensitivity_metric =
        speed_difference > 0
            ? "ChromeOS.Settings.Device.PointingStick.Sensitivity.Increase"
            : "ChromeOS.Settings.Device.PointingStick.Sensitivity.Decrease";
    base::UmaHistogramEnumeration(
        delta_sensitivity_metric,
        static_cast<PointerSensitivity>(abs(speed_difference)));
  }
  if (pointing_stick.settings->swap_right != old_settings.swap_right) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.PointingStick.SwapPrimaryButtons.Changed",
        pointing_stick.settings->swap_right);
  }
  HandleSettingsUpdatedMetric(pointing_stick);
}

void InputDeviceSettingsMetricsManager::RecordTouchpadInitialMetrics(
    const mojom::Touchpad& touchpad) {
  // Only record the metrics once for each Touchpad.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_touchpads_.find(account_id);

  if (iter != recorded_touchpads_.end() &&
      base::Contains(iter->second, touchpad.device_key)) {
    return;
  }
  recorded_touchpads_[account_id].insert(touchpad.device_key);

  const std::string touchpad_metrics_prefix =
      touchpad.is_external ? "ChromeOS.Settings.Device.Touchpad.External."
                           : "ChromeOS.Settings.Device.Touchpad.Internal.";
  PointerSensitivity sensitivity =
      static_cast<PointerSensitivity>(touchpad.settings->sensitivity);

  base::UmaHistogramBoolean(
      touchpad_metrics_prefix + "AccelerationEnabled.Initial",
      touchpad.settings->acceleration_enabled);
  base::UmaHistogramBoolean(
      touchpad_metrics_prefix + "ReverseScrolling.Initial",
      touchpad.settings->reverse_scrolling);
  base::UmaHistogramEnumeration(touchpad_metrics_prefix + "Sensitivity.Initial",
                                sensitivity);
  base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapDragging.Initial",
                            touchpad.settings->tap_dragging_enabled);
  base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapToClick.Initial",
                            touchpad.settings->tap_to_click_enabled);

  if (touchpad.is_haptic) {
    PointerSensitivity haptic_sensitivity =
        static_cast<PointerSensitivity>(touchpad.settings->haptic_sensitivity);
    base::UmaHistogramBoolean(touchpad_metrics_prefix + "HapticEnabled.Initial",
                              touchpad.settings->haptic_enabled);
    base::UmaHistogramEnumeration(
        touchpad_metrics_prefix + "HapticSensitivity.Initial",
        haptic_sensitivity);
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    base::UmaHistogramEnumeration(
        touchpad_metrics_prefix + "SimulateRightClick.Initial",
        touchpad.settings->simulate_right_click);
  }
}

void InputDeviceSettingsMetricsManager::RecordTouchpadChangedMetrics(
    const mojom::Touchpad& touchpad,
    const mojom::TouchpadSettings& old_settings) {
  const std::string touchpad_metrics_prefix =
      touchpad.is_external ? "ChromeOS.Settings.Device.Touchpad.External."
                           : "ChromeOS.Settings.Device.Touchpad.Internal.";
  if (touchpad.settings->acceleration_enabled !=
      old_settings.acceleration_enabled) {
    base::UmaHistogramBoolean(
        touchpad_metrics_prefix + "AccelerationEnabled.Changed",
        touchpad.settings->acceleration_enabled);
  }
  if (touchpad.settings->reverse_scrolling != old_settings.reverse_scrolling) {
    base::UmaHistogramBoolean(
        touchpad_metrics_prefix + "ReverseScrolling.Changed",
        touchpad.settings->reverse_scrolling);
  }
  if (touchpad.settings->sensitivity != old_settings.sensitivity) {
    PointerSensitivity sensitivity =
        static_cast<PointerSensitivity>(touchpad.settings->sensitivity);
    base::UmaHistogramEnumeration(
        touchpad_metrics_prefix + "Sensitivity.Changed", sensitivity);
    const int speed_difference =
        touchpad.settings->sensitivity - old_settings.sensitivity;
    const std::string delta_metric_suffix =
        speed_difference > 0 ? "Sensitivity.Increase" : "Sensitivity.Decrease";
    base::UmaHistogramEnumeration(
        base::StrCat({touchpad_metrics_prefix, delta_metric_suffix}),
        static_cast<PointerSensitivity>(abs(speed_difference)));
  }
  if (touchpad.settings->tap_dragging_enabled !=
      old_settings.tap_dragging_enabled) {
    base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapDragging.Changed",
                              touchpad.settings->tap_dragging_enabled);
  }
  if (touchpad.settings->tap_to_click_enabled !=
      old_settings.tap_to_click_enabled) {
    base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapToClick.Changed",
                              touchpad.settings->tap_to_click_enabled);
  }
  if (touchpad.is_haptic) {
    if (touchpad.settings->haptic_enabled != old_settings.haptic_enabled) {
      bool haptic_enabled = touchpad.settings->haptic_enabled;
      base::UmaHistogramBoolean(
          touchpad_metrics_prefix + "HapticEnabled.Changed", haptic_enabled);
    }
    if (touchpad.settings->haptic_sensitivity !=
        old_settings.haptic_sensitivity) {
      PointerSensitivity haptic_sensitivity = static_cast<PointerSensitivity>(
          touchpad.settings->haptic_sensitivity);
      base::UmaHistogramEnumeration(
          touchpad_metrics_prefix + "HapticSensitivity.Changed",
          haptic_sensitivity);
      const int speed_difference = touchpad.settings->haptic_sensitivity -
                                   old_settings.haptic_sensitivity;
      const std::string delta_metric_suffix =
          speed_difference > 0 ? "HapticSensitivity.Increase"
                               : "HapticSensitivity.Decrease";
      base::UmaHistogramEnumeration(
          base::StrCat({touchpad_metrics_prefix, delta_metric_suffix}),
          static_cast<PointerSensitivity>(abs(speed_difference)));
    }
  }
  if (features::IsAltClickAndSixPackCustomizationEnabled() &&
      touchpad.settings->simulate_right_click !=
          old_settings.simulate_right_click) {
    base::UmaHistogramEnumeration(
        touchpad_metrics_prefix + "SimulateRightClick.Changed",
        touchpad.settings->simulate_right_click);
  }
  HandleSettingsUpdatedMetric(touchpad);
}

void InputDeviceSettingsMetricsManager::RecordGraphicsTabletInitialMetrics(
    const mojom::GraphicsTablet& graphics_tablet) {
  // Only record the metrics once for each graphics tablet.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_graphics_tablets_.find(account_id);

  if (iter != recorded_graphics_tablets_.end() &&
      base::Contains(iter->second, graphics_tablet.device_key)) {
    return;
  }
  recorded_graphics_tablets_[account_id].insert(graphics_tablet.device_key);

  for (const auto& button_remapping :
       graphics_tablet.settings->pen_button_remappings) {
    RecordCurrentButtonRemappingAction(button_remapping,
                                       /*peripheral_kind=*/"GraphicsTabletPen");
  }
  for (const auto& button_remapping :
       graphics_tablet.settings->tablet_button_remappings) {
    RecordCurrentButtonRemappingAction(button_remapping,
                                       /*peripheral_kind=*/"GraphicsTablet");
  }
}

void InputDeviceSettingsMetricsManager::RecordGraphicsTabletChangedMetrics(
    const mojom::GraphicsTablet& graphics_tablet,
    const mojom::GraphicsTabletSettings& old_settings) {
  for (const auto& new_remapping :
       graphics_tablet.settings->pen_button_remappings) {
    for (const auto& original_remapping : old_settings.pen_button_remappings) {
      if (original_remapping->button == new_remapping->button) {
        RecordButtonRemappingNameIfChanged(original_remapping, new_remapping,
                                           /*peripheral_kind=*/
                                           "GraphicsTabletPen");
        RecordButtonRemappingActionIfChanged(original_remapping, new_remapping,
                                             /*peripheral_kind=*/
                                             "GraphicsTabletPen");
      }
    }
  }
  for (const auto& new_remapping :
       graphics_tablet.settings->tablet_button_remappings) {
    for (const auto& original_remapping :
         old_settings.tablet_button_remappings) {
      if (original_remapping->button == new_remapping->button) {
        RecordButtonRemappingNameIfChanged(original_remapping, new_remapping,
                                           /*peripheral_kind=*/
                                           "GraphicsTablet");
        RecordButtonRemappingActionIfChanged(original_remapping, new_remapping,
                                             /*peripheral_kind=*/
                                             "GraphicsTablet");
      }
    }
  }
}

void InputDeviceSettingsMetricsManager::RecordModifierRemappingHash(
    const mojom::Keyboard& keyboard) {
  // Compute hash by left-shifting by `kModifierHashWidth` and then inserting
  // the modifier value from prefs at into the lowest `kModifierHashWidth` bits.
  uint32_t hash = 0;
  for (ssize_t i = kNumModifiers - 1u; i >= 0; i--) {
    const auto modifier_key = kModifierNames[i].modifier_key;
    auto iter = keyboard.settings->modifier_remappings.find(modifier_key);
    const auto remapped_key_value =
        iter == keyboard.settings->modifier_remappings.end()
            ? static_cast<uint32_t>(modifier_key)
            : static_cast<uint32_t>(iter->second);

    // Check that shifting and adding value will not overflow `hash`.
    DCHECK(remapped_key_value <= kMaxModifierValue && remapped_key_value >= 0);
    DCHECK(hash < (1u << ((sizeof(uint32_t) * 8u) - kModifierHashWidth)));

    hash <<= kModifierHashWidth;
    hash += static_cast<uint32_t>(remapped_key_value);
  }

  // If the computed hash matches the hash when settings are in a default state,
  // the metric should not be published.
  if (hash != kDefaultModifierHash) {
    const std::string metrics =
        base::StrCat({GetKeyboardMetricsPrefix(keyboard), "Modifiers.Hash"});
    base::UmaHistogramSparse(metrics, static_cast<int>(hash));
  }
}

void InputDeviceSettingsMetricsManager::RecordSixPackKeyInfo(
    const mojom::Keyboard& keyboard,
    ui::KeyboardCode key_code,
    bool is_initial_value) {
  base::UmaHistogramEnumeration(
      GetSixPackKeyMetricName(GetKeyboardMetricsPrefix(keyboard), key_code,
                              is_initial_value),
      GetSixPackKeyModifier(keyboard, key_code));
}

void InputDeviceSettingsMetricsManager::RecordKeyboardMouseComboDeviceMetric(
    const mojom::Keyboard& keyboard,
    const mojom::Mouse& mouse) {
  static base::NoDestructor<base::flat_set<std::string>> logged_devices;
  static constexpr auto kKnownKeyboardMouseComboDevices =
      base::MakeFixedFlatSet<base::StringPiece>({
          "046d:4024",  // Logitech K400
          "046d:404d",  // Logitech K400+
          "046d:c548",  // Logitech BOLT Receiver
          "17ef:60e1",  // Lenovo TrackPoint Keyboard II
          "17ef:60ee",  // Lenovo TrackPoint Keyboard II
          "17ef:609f",  // Lenovo 100 USB-A Wireless Combo Keyboard and Mouse
      });

  auto [_, inserted] = logged_devices->insert(keyboard.device_key);
  if (!inserted) {
    return;
  }

  if (kKnownKeyboardMouseComboDevices.contains(keyboard.device_key)) {
    base::UmaHistogramEnumeration(
        "ChromeOS.Inputs.ComboDeviceClassification",
        ui::ComboDeviceClassification::kKnownComboDevice);
  } else {
    LOG(WARNING) << base::StringPrintf(
        "Classification for combo device '%s' with identifier '%s' is "
        "unknown.",
        keyboard.name.c_str(), keyboard.device_key.c_str());
    base::UmaHistogramEnumeration("ChromeOS.Inputs.ComboDeviceClassification",
                                  ui::ComboDeviceClassification::kUnknown);
  }
}

void InputDeviceSettingsMetricsManager::RecordNewButtonRegisteredMetrics(
    const mojom::Button& button,
    const char* peripheral_kind) {
  const std::string metric_name_prefix =
      base::StrCat({"ChromeOS.Settings.Device.", peripheral_kind,
                    ".ButtonRemapping.Registered."});
  if (button.is_customizable_button()) {
    base::UmaHistogramEnumeration(
        base::StrCat({metric_name_prefix, "CustomizableButton"}),
        button.get_customizable_button());
  } else if (button.is_vkey()) {
    base::UmaHistogramSparse(base::StrCat({metric_name_prefix, "Vkey"}),
                             button.get_vkey());
  }
}

}  // namespace ash
