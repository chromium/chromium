// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_UTILS_H_

#include "chromeos/crosapi/mojom/cros_display_config.mojom-forward.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"
#include "ui/gfx/geometry/insets.h"

namespace extensions {

// Callback function for CrosDisplayConfigController crosapi interface.
// Converts input display layout |info| from crosapi to extension api type.
// Passes converted array into a |callback|.
void OnGetDisplayLayoutResult(
    base::OnceCallback<void(DisplayInfoProvider::DisplayLayoutList)> callback,
    crosapi::mojom::DisplayLayoutInfoPtr info);

// Converts display id string to number.
// Returns invalid id in case of error.
int64_t GetDisplayId(const std::string& display_id_str);

// Returns a display object for if display id is found.
// Return empty display object otherwise.
display::Display GetDisplayForId(const std::string& display_id_str);

// Converts display layout |position| from extension api to crosapi type.
crosapi::mojom::DisplayLayoutPosition GetDisplayLayoutPosition(
    api::system_display::LayoutPosition position);

// Converts system display |insets| to gfx type.
gfx::Insets GetInsets(const api::system_display::Insets& insets);

// Converts int |rotation_value| to crosapi type.
crosapi::mojom::DisplayRotationOptions GetMojomDisplayRotationOptions(
    int rotation_value);

// Converts crosapi |rotation_options| to int.
int GetRotationFromMojomDisplayRotationInfo(
    crosapi::mojom::DisplayRotationOptions rotation_options);

// Validates the DisplayProperties input. Does not perform any tests with
// DisplayManager dependencies. Returns an error string on failure or nullopt
// on success.
std::optional<std::string> ValidateDisplayPropertiesInput(
    const std::string& display_id_str,
    const api::system_display::DisplayProperties& info);

// Converts display unit crosapi |mojo_info| to system display type.
api::system_display::DisplayUnitInfo GetDisplayUnitInfoFromMojo(
    const crosapi::mojom::DisplayUnitInfo& mojo_info);

// Converts system display calibration |pair| to crosapi type.
crosapi::mojom::TouchCalibrationPairPtr GetTouchCalibrationPair(
    const api::system_display::TouchCalibrationPair& pair);

void SetDisplayUnitInfoLayoutProperties(
    const crosapi::mojom::DisplayLayoutInfo& layout,
    api::system_display::DisplayUnitInfo* display);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_UTILS_H_
