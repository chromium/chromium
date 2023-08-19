// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS_ROUTES_UTIL_H_
#define ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS_ROUTES_UTIL_H_

#include <string>

#include "base/component_export.h"

namespace chromeos::settings {

// Returns true if the sub-page is one defined in `routes.mojom`.
COMPONENT_EXPORT(ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS)
bool IsOSSettingsSubPage(const std::string& sub_page);

}  // namespace chromeos::settings

#endif  // ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS_ROUTES_UTIL_H_
