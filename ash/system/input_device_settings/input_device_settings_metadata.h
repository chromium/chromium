// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/flat_map.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

enum class DeviceType {
  kUnknown = 0,
  kKeyboard = 1,
  kKeyboardMouseCombo = 2,
  kMouse = 3,
};

enum class MetadataTier {
  kNoMetadata = 0,
  kClassificationOnly = 1,
  kHasButtonConfig = 2,
  kMaxValue = kHasButtonConfig,
};

enum class DeviceImageDestination {
  kNotification = 0,
  kSettings = 1,
};

struct ASH_EXPORT MouseMetadata {
  MouseMetadata();
  MouseMetadata(mojom::CustomizationRestriction customization_restriction,
                mojom::MouseButtonConfig config,
                std::optional<std::string> name = std::nullopt);
  ~MouseMetadata();
  MouseMetadata(const MouseMetadata& other);

  mojom::CustomizationRestriction customization_restriction;
  mojom::MouseButtonConfig mouse_button_config;
  std::optional<std::string> name = std::nullopt;
  bool operator==(const MouseMetadata& other) const;
};

struct ASH_EXPORT GraphicsTabletMetadata {
  GraphicsTabletMetadata();
  GraphicsTabletMetadata(
      mojom::CustomizationRestriction customization_restriction,
      mojom::GraphicsTabletButtonConfig config,
      std::optional<std::string> name = std::nullopt);
  ~GraphicsTabletMetadata();
  GraphicsTabletMetadata(const GraphicsTabletMetadata& other);

  mojom::CustomizationRestriction customization_restriction;
  mojom::GraphicsTabletButtonConfig graphics_tablet_button_config;
  std::optional<std::string> name = std::nullopt;
  bool operator==(const GraphicsTabletMetadata& other) const;
};

struct ASH_EXPORT KeyboardMetadata {};

struct ASH_EXPORT KeyboardMouseComboMetadata {
  mojom::CustomizationRestriction customization_restriction;
  bool operator==(const KeyboardMouseComboMetadata& other) const;
};

// This function returns mouse metadata. Returns nullptr if there is no metadata
// on the mouse.
ASH_EXPORT const MouseMetadata* GetMouseMetadata(const ui::InputDevice& device);

// This function returns graphics tablet metadata. Returns nullptr if there is
// no metadata on the graphics tablet.
ASH_EXPORT const GraphicsTabletMetadata* GetGraphicsTabletMetadata(
    const ui::InputDevice& device);

// This function returns keyboard metadata. Returns nullptr if there is no
// metadata on the keyboard.
ASH_EXPORT const KeyboardMetadata* GetKeyboardMetadata(
    const ui::InputDevice& device);

// This function returns keyboard mouse combo metadata. Returns nullptr if there
// is no metadata on the keyboard mouse combo.
ASH_EXPORT const KeyboardMouseComboMetadata* GetKeyboardMouseComboMetadata(
    const ui::InputDevice& device);

// This function returns the device type of the input device.
ASH_EXPORT DeviceType GetDeviceType(const ui::InputDevice& device);

// This function returns the mouse metadata list.
ASH_EXPORT const base::flat_map<VendorProductId, MouseMetadata>&
GetMouseMetadataList();

// This function returns the graphics tablet metadata list.
ASH_EXPORT const base::flat_map<VendorProductId, GraphicsTabletMetadata>&
GetGraphicsTabletMetadataList();

// This function returns the keyboard mouse combo metadata list.
ASH_EXPORT const base::flat_map<VendorProductId, KeyboardMouseComboMetadata>&
GetKeyboardMouseComboMetadataList();

// This function returns the keyboard metadata list.
ASH_EXPORT const base::flat_map<VendorProductId, KeyboardMetadata>&
GetKeyboardMetadataList();

// This function returns the button remapping list from the peripherals.
ASH_EXPORT std::vector<mojom::ButtonRemappingPtr>
GetButtonRemappingListForConfig(mojom::MouseButtonConfig mouse_button_config);

// This function returns the button remapping list for pen buttons based on the
// config.
ASH_EXPORT std::vector<mojom::ButtonRemappingPtr>
GetPenButtonRemappingListForConfig(
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config);

// This function returns the button remapping list for tablet buttons based on
// the config.
ASH_EXPORT std::vector<mojom::ButtonRemappingPtr>
GetTabletButtonRemappingListForConfig(
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config);

// This function returns the vid pid alias list.
ASH_EXPORT const base::flat_map<VendorProductId, VendorProductId>&
GetVidPidAliasList();

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_H_
