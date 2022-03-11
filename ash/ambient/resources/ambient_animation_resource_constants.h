// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_RESOURCE_CONSTANTS_H_
#define ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_RESOURCE_CONSTANTS_H_

#include <array>

#include "base/strings/string_piece.h"

namespace ash {
namespace ambient {
namespace resources {

// Free the breeze.
inline constexpr base::StringPiece kClipBottomAssetId = "clip_bottom.png";
inline constexpr base::StringPiece kClipTopAssetId = "clip_top.png";
inline constexpr base::StringPiece kFrameImage1AssetId = "frame_image_1.png";
inline constexpr base::StringPiece kFrameImage2AssetId = "frame_image_2.png";
inline constexpr base::StringPiece kTreeShadowAssetId = "tree_shadow.png";
inline constexpr int kFeelTheBreezeNumStaticAssets = 5;
inline constexpr std::array<base::StringPiece, kFeelTheBreezeNumStaticAssets>
    kAllFeelTheBreezeStaticAssets = {kClipBottomAssetId, kClipTopAssetId,
                                     kFrameImage1AssetId, kFrameImage2AssetId,
                                     kTreeShadowAssetId};

}  // namespace resources
}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_RESOURCE_CONSTANTS_H_
