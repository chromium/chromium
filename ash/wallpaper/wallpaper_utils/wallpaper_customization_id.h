// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_CUSTOMIZATION_ID_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_CUSTOMIZATION_ID_H_

#include <optional>
#include <string_view>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Reply with the customization_id from machine statistics. Some testing setups
// will not have a customization_id, so the callback will frequently be called
// with null in tests and development.
ASH_EXPORT void GetCustomizationId(
    base::OnceCallback<void(std::optional<std::string_view>)>
        get_customization_id_callback);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_CUSTOMIZATION_ID_H_
