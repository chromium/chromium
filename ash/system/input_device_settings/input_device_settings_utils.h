// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_

#include <string_view>

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/export_template.h"
#include "base/values.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/devices/input_device.h"
#include "ui/gfx/image/image.h"

class AccountId;
class PrefService;

namespace ash {

struct VendorProductId {
  uint16_t vendor_id;
  uint16_t product_id;
  constexpr bool operator<(const VendorProductId& other) const {
    return vendor_id == other.vendor_id ? product_id < other.product_id
                                        : vendor_id < other.vendor_id;
  }
  bool operator==(const VendorProductId& other) const;
};

// Checks if a given value is within the bounds set by `ui::mojom::ModifierKey`.
bool IsValidModifier(int val);

// Builds `device_key` for use in storing device settings in prefs.
ASH_EXPORT std::string BuildDeviceKey(const ui::InputDevice& device);
// Builds a unique device key string based on vendor and product IDs.
ASH_EXPORT std::string BuildDeviceKey(uint16_t vendor_id, uint16_t product_id);
// Decides based on the existing settings storage and default value if the given
// setting should be persisted.
// Settings should be persisted if any of the following are true:
// - Setting was previously persisted to storage
// - Setting is different than the default, which means the user manually
//   changed the value.
// - `force_persistence` requires the setting to be persisted, this means this
//   device is being transitioned from the old global settings to per-device
//   settings and the user specified the specific value for this setting.
template <typename T>
bool ShouldPersistSetting(std::string_view setting_key,
                          T new_value,
                          T default_value,
                          bool force_persistence,
                          const ::base::Value::Dict* existing_settings_dict);

// Decides based on the policy, existing settings storage, and default value if
// the given setting should be persisted. Settings should be persisted if any of
// the following are true when there is no policy:
// - Setting was previously persisted to storage
// - Setting is different than the default, which means the user manually
//   changed the value.
// - `force_persistence` requires the setting to be persisted, this means this
//   device is being transitioned from the old global settings to per-device
//   settings and the user specified the specific value for this setting.

// If there is a managed policy, settings should only be persisted if
// `force_peristence` is true.
// If there is a recommended policy, settings should be persisted if any of the
// following are true:
// - Setting was previously persisted to storage
// - Setting is going to be set to a different value than the policy recommended
// value.
ASH_EXPORT bool ShouldPersistSetting(
    const mojom::InputDeviceSettingsPolicyPtr& policy,
    std::string_view setting_key,
    bool new_value,
    bool default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

ASH_EXPORT bool ShouldPersistFkeySetting(
    const mojom::InputDeviceSettingsFkeyPolicyPtr& policy,
    std::string_view setting_key,
    std::optional<ui::mojom::ExtendedFkeysModifier> new_value,
    ui::mojom::ExtendedFkeysModifier default_value,
    const base::Value::Dict* existing_settings_dict);

// Templates exported for each valid value type.
extern template EXPORT_TEMPLATE_DECLARE(ASH_EXPORT) bool ShouldPersistSetting(
    std::string_view setting_key,
    bool new_value,
    bool default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

extern template EXPORT_TEMPLATE_DECLARE(ASH_EXPORT) bool ShouldPersistSetting(
    std::string_view setting_key,
    int new_value,
    int default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

// Retrieve cached internal/external device settings dictionary (if it exists).
ASH_EXPORT const base::Value::Dict* GetLoginScreenSettingsDict(
    PrefService* local_state,
    AccountId account_id,
    const std::string& pref_name);

// Retrieve cached button remapping list (if it exists).
ASH_EXPORT const base::Value::List* GetLoginScreenButtonRemappingList(
    PrefService* local_state,
    AccountId account_id,
    const std::string& pref_name);

// These two functions are used to convert the button remapping or dict
// in order to save the mojom object to the prefs as a dict.
ASH_EXPORT base::Value::Dict ConvertButtonRemappingToDict(
    const mojom::ButtonRemapping& remapping,
    mojom::CustomizationRestriction customization_restriction,
    bool redact_button_names = false);
ASH_EXPORT mojom::ButtonRemappingPtr ConvertDictToButtonRemapping(
    const base::Value::Dict& dict,
    mojom::CustomizationRestriction customization_restriction);

// This helper function converts the button remapping object array
// to a list of dicts to be stored in prefs.
ASH_EXPORT base::Value::List ConvertButtonRemappingArrayToList(
    const std::vector<mojom::ButtonRemappingPtr>& remappings,
    mojom::CustomizationRestriction customization_restriction,
    bool redact_button_names = false);

// This helper function converts a list of dicts to
// a button remapping object array. The dicts will be stored in prefs.
ASH_EXPORT std::vector<mojom::ButtonRemappingPtr>
ConvertListToButtonRemappingArray(
    const base::Value::List& list,
    mojom::CustomizationRestriction customization_restriction);

// Returns whether the given keyboard is ChromeOS layout keyboard.
ASH_EXPORT bool IsChromeOSKeyboard(const mojom::Keyboard& keyboard);

// Returns whether the given keyboard is a split modifier keyboard.
ASH_EXPORT bool IsSplitModifierKeyboard(const mojom::Keyboard& keyboard);
ASH_EXPORT bool IsSplitModifierKeyboard(int keyboard_id);

// Rewrites `device_key` to a known, supported device key if the
// `kWelcomeExperienceTestUnsupportedDevices` flag is enabled.
ASH_EXPORT std::string GetDeviceKeyForMetadataRequest(
    const std::string& device_key);

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_
