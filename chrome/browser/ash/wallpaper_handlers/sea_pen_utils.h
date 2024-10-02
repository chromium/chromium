// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_UTILS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_UTILS_H_

#include <optional>
#include <string_view>

#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "components/manta/proto/manta.pb.h"
#include "ui/gfx/geometry/size.h"

inline constexpr std::string_view kTemplateIdTag =
    "chromeos_wallpaper_template_id";

namespace wallpaper_handlers {

// Returns the size in pixels of the largest display by area. If the display is
// in portrait mode (taller than wide) the display size is transposed to always
// be landscape (wider than tall).
gfx::Size GetLargestDisplaySizeLandscape();

// Helper function to validate the Manta API output data.
bool IsValidOutput(const manta::proto::OutputData& output,
                   std::string_view source);

// Common helper function between `FetchThumbnails` and `FetchWallpaper`.
manta::proto::Request CreateMantaRequest(
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    std::optional<uint32_t> generation_seed,
    int num_outputs,
    const gfx::Size& size,
    manta::proto::FeatureName feature_name);

std::string GetFeedbackText(
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    const ash::personalization_app::mojom::SeaPenFeedbackMetadataPtr& metadata);

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_UTILS_H_
