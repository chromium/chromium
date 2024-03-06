// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_UTILS_GENERATED_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_UTILS_GENERATED_H_

#include <string>

#include "ash/webui/common/mojom/sea_pen.mojom.h"

namespace wallpaper_handlers {

std::string TemplateIdToString(
    ash::personalization_app::mojom::SeaPenTemplateId id);

std::string TemplateChipToString(
    ash::personalization_app::mojom::SeaPenTemplateChip chip);

std::string TemplateOptionToString(
    ash::personalization_app::mojom::SeaPenTemplateOption option);

const std::map<ash::personalization_app::mojom::SeaPenTemplateId,
               std::set<ash::personalization_app::mojom::SeaPenTemplateChip>>&
TemplateToChipSet();

const std::map<ash::personalization_app::mojom::SeaPenTemplateChip,
               std::set<ash::personalization_app::mojom::SeaPenTemplateOption>>&
ChipToOptionSet();

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_UTILS_GENERATED_H_
