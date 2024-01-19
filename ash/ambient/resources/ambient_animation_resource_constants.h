// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_RESOURCE_CONSTANTS_H_
#define ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_RESOURCE_CONSTANTS_H_

#include <array>
#include <string_view>

namespace ash {
namespace ambient {
namespace resources {

// Feel the breeze.
inline constexpr std::string_view kClipBottomAssetId = "clip_bottom.png";
inline constexpr std::string_view kClipTopAssetId = "clip_top.png";
inline constexpr std::string_view kFrameImage1AssetId = "frame_image_1.png";
inline constexpr std::string_view kFrameImage2AssetId = "frame_image_2.png";
inline constexpr std::string_view kTreeShadowAssetId = "tree_shadow.png";
inline constexpr std::string_view kStringAssetId = "string.png";
inline constexpr int kFeelTheBreezeNumStaticAssets = 6;
inline constexpr std::array<std::string_view, kFeelTheBreezeNumStaticAssets>
    kAllFeelTheBreezeStaticAssets = {kClipBottomAssetId,  kClipTopAssetId,
                                     kFrameImage1AssetId, kFrameImage2AssetId,
                                     kTreeShadowAssetId,  kStringAssetId};

// Float on by.
inline constexpr std::string_view kShadowA1AssetId = "shadow_a_1.png";
inline constexpr std::string_view kShadowB1AssetId = "shadow_b_1.png";
inline constexpr std::string_view kShadowC1AssetId = "shadow_c_1.png";
inline constexpr std::string_view kShadowD1AssetId = "shadow_d_1.png";
inline constexpr std::string_view kShadowE1AssetId = "shadow_e_1.png";
inline constexpr std::string_view kShadowF1AssetId = "shadow_f_1.png";
inline constexpr std::string_view kShadowG1AssetId = "shadow_g_1.png";
inline constexpr std::string_view kShadowH1AssetId = "shadow_h_1.png";
inline constexpr int kFloatOnByNumStaticAssets = 8;
inline constexpr std::array<std::string_view, kFloatOnByNumStaticAssets>
    kAllFloatOnByStaticAssets = {
        kShadowA1AssetId, kShadowB1AssetId, kShadowC1AssetId, kShadowD1AssetId,
        kShadowE1AssetId, kShadowF1AssetId, kShadowG1AssetId, kShadowH1AssetId,
};

}  // namespace resources
}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_RESOURCE_CONSTANTS_H_
