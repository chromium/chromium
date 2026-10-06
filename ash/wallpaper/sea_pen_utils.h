// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_SEA_PEN_UTILS_H_
#define ASH_WALLPAPER_SEA_PEN_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "components/manta/proto/manta.pb.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

inline constexpr std::string_view kTemplateIdTag = "template_id";

// Returns the size in pixels of the largest display by area. If the display is
// in portrait mode (taller than wide) the display size is transposed to always
// be landscape (wider than tall).
ASH_EXPORT gfx::Size GetLargestDisplaySizeLandscape();

// Helper function to validate the Manta API output data.
ASH_EXPORT bool IsValidOutput(const manta::proto::OutputData& output,
                              std::string_view source);

// Common helper function between `FetchThumbnails` and `FetchWallpaper`.
ASH_EXPORT manta::proto::Request CreateMantaRequest(
    const personalization_app::mojom::SeaPenQueryPtr& query,
    std::optional<uint32_t> generation_seed,
    int num_outputs,
    const gfx::Size& size,
    manta::proto::FeatureName feature_name);

ASH_EXPORT std::string GetFeedbackText(
    const personalization_app::mojom::SeaPenQueryPtr& query,
    const personalization_app::mojom::SeaPenFeedbackMetadataPtr& metadata);

}  // namespace ash

#endif  // ASH_WALLPAPER_SEA_PEN_UTILS_H_
