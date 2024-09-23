// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_UTILS_GENERATED_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_UTILS_GENERATED_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"

namespace ash {

ASH_EXPORT std::string TemplateIdToString(
    ash::personalization_app::mojom::SeaPenTemplateId id);

ASH_EXPORT std::string TemplateChipToString(
    ash::personalization_app::mojom::SeaPenTemplateChip chip);

ASH_EXPORT std::string TemplateOptionToString(
    ash::personalization_app::mojom::SeaPenTemplateOption option);

ASH_EXPORT const std::map<ash::personalization_app::mojom::SeaPenTemplateId,
               std::set<ash::personalization_app::mojom::SeaPenTemplateChip>>&
TemplateToChipSet();

ASH_EXPORT const std::map<ash::personalization_app::mojom::SeaPenTemplateChip,
               std::set<ash::personalization_app::mojom::SeaPenTemplateOption>>&
ChipToOptionSet();

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_UTILS_GENERATED_H_
