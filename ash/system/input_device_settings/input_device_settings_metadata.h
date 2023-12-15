// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ui/events/devices/input_device.h"

namespace ash {

struct ASH_EXPORT MouseMetadata {
  mojom::CustomizationRestriction customization_restriction;
  bool operator==(const MouseMetadata& other) const;
};

// This function returns mouse metadata. Returns nullptr if there is no metadata
// on the mouse.
ASH_EXPORT const MouseMetadata* GetMouseMetadata(const ui::InputDevice& device);

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_H_
