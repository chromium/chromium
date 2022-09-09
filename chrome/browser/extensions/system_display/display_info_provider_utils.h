// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_UTILS_H_

#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "extensions/browser/api/system_display/display_info_provider.h"

namespace extensions {

// Callback function for CrosDisplayConfigController crosapi interface.
// Reused by both ash and lacros implementations of DisplayInfoProvider.
// Converts input display layout |info| from crosapi to extension api type.
// Passes converted array into a |callback|.
void OnGetDisplayLayoutResult(
    base::OnceCallback<void(DisplayInfoProvider::DisplayLayoutList)> callback,
    crosapi::mojom::DisplayLayoutInfoPtr info);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_UTILS_H_
