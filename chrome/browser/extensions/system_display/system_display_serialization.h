// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_SYSTEM_DISPLAY_SERIALIZATION_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_SYSTEM_DISPLAY_SERIALIZATION_H_

#include "base/component_export.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"

// This file declares utilities to serialize / deserialize system_display.idl
// structs under
//   extensions::api::system_display::
// to / from Stable crosapi Mojo system_display structs under
//   crosapi::mojom::
// which needs to be Stable and need to handle version skew.

// Approach: Use serialize / deserialize function pairs:
// * Serialize: Converts extensions::api::system_display objects to
//   crosapi::mojom (include possible StructTrait mapped) objects. The results
//   are returned by value, or are allocated and returned as Mojo Ptr.
// * Deserialize: Reads crosapi::mojom (include possible StructTraits mapped)
//   objects and writes to extensions::api::system_display objects or fields.
//   The deserialized objects are written via |dst| pointer(s), to accommodate a
//   wider variety of deserialized values. Also, it's the caller's
//   responsibility to check the presence of serialized |src| data, and allocate
//   |dst| data as needed.

// Rejected alternatives:
// * Use extensions::api::system_display objects' generated ToValue() and
//   FromValue() functions: Unfortunately, FromValue() is unforgiving, so it
//   won't handle version skews. If a less stringent API becomes available
//   (e.g., FromValueAllowMissing()) then we can revisit this alternative.
// * Use ash/public/mojom/cros_display_config.mojom: These are not meant to be
//   Stable, and deviate from system_display.idl.
// * Use StructTraits to map crosapi Mojo to extensions::api::system_display::
//   objects: This is viable, but the resulting code can be bulky. The function
//   pair approach is more direct (i.e., less prone to leakage of abstraction),
//   and allows matching serialization / deserialization code to be in
//   close proximity, so future modifications and version skew handling code
//   can be better juxtaposed.

namespace extensions {
namespace api {
namespace system_display {

struct DisplayMode;
struct DisplayUnitInfo;
struct Edid;

// extensions::api::system_display::DisplayMode <==>
//     crosapi::mojom::DisplayMode.

COMPONENT_EXPORT(CROSAPI)
crosapi::mojom::DisplayModePtr SerializeDisplayMode(
    const extensions::api::system_display::DisplayMode& src);

COMPONENT_EXPORT(CROSAPI)
void DeserializeDisplayMode(const crosapi::mojom::DisplayMode& src,
                            extensions::api::system_display::DisplayMode* dst);

// extensions::api::system_display::Edid <==> crosapi::mojom::Edid.

COMPONENT_EXPORT(CROSAPI)
crosapi::mojom::EdidPtr SerializeEdid(
    const extensions::api::system_display::Edid& src);

COMPONENT_EXPORT(CROSAPI)
void DeserializeEdid(const crosapi::mojom::Edid& src,
                     extensions::api::system_display::Edid* dst);

// extensions::api::system_display::DisplayUnitInfo <==>
//     crosapi::mojom::DisplayUnitInfo.

COMPONENT_EXPORT(CROSAPI)
crosapi::mojom::DisplayUnitInfoPtr SerializeDisplayUnitInfo(
    const extensions::api::system_display::DisplayUnitInfo& src);

COMPONENT_EXPORT(CROSAPI)
void DeserializeDisplayUnitInfo(
    const crosapi::mojom::DisplayUnitInfo& src,
    extensions::api::system_display::DisplayUnitInfo* dst);

}  // namespace system_display
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_SYSTEM_DISPLAY_SERIALIZATION_H_
