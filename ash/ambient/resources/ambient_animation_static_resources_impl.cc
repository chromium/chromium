// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include <utility>

#include "ash/ambient/resources/grit/ash_ambient_lottie_resources.h"
#include "base/check.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

using AnimationThemeToResourceIdMap =
    base::flat_map<AmbientAnimationTheme, int>;
using AssetIdToResourceIdMap = base::flat_map<base::StringPiece, int>;

const AnimationThemeToResourceIdMap& GetAnimationThemeToLottieResourceIdMap() {
  static const AnimationThemeToResourceIdMap* m =
      new AnimationThemeToResourceIdMap(
          {{AmbientAnimationTheme::kFeelTheBreeze,
            IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_ANIMATION_JSON}});
  return *m;
}

// TODO(esum): Look into auto-generating this map and the one above via a
// build-time script.
AssetIdToResourceIdMap GetAssetIdToResourceIdMapForTheme(
    AmbientAnimationTheme theme) {
  base::flat_map<AmbientAnimationTheme, AssetIdToResourceIdMap> m = {
      // Themes
      {
          // Theme: Feel the Breeze
          AmbientAnimationTheme::kFeelTheBreeze,
          {
              // Assets
              {"bg.png", IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_BG_PNG},
              {"clips_bottom.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_CLIPS_BOTTOM_PNG},
              {"clips_top.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_CLIPS_TOP_PNG},
              {"shadow_1.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_SHADOW_1_PNG},
              {"shadow_2.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_SHADOW_2_PNG},
              {"shadow_3.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_SHADOW_3_PNG},
              {"shadow_4.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_SHADOW_4_PNG},
              {"shadow_5.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_SHADOW_5_PNG},
              {"shadow_6.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_SHADOW_6_PNG},
              {"time_weather.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_TIME_WEATHER_PNG},
              {"tree_shadow.png",
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_TREE_SHADOW_PNG}
              // End Assets
          }
          // End Theme: Feel the Breeze
      }
      // End Themes
  };
  DCHECK(m.contains(theme)) << "Asset/resource ids missing for " << theme;
  return m.at(theme);
}

class AmbientAnimationStaticResourcesImpl
    : public AmbientAnimationStaticResources {
 public:
  AmbientAnimationStaticResourcesImpl(
      int lottie_json_resource_id,
      base::flat_map<base::StringPiece, int> asset_id_to_resource_id)
      : lottie_json_resource_id_(lottie_json_resource_id),
        asset_id_to_resource_id_(std::move(asset_id_to_resource_id)) {}

  AmbientAnimationStaticResourcesImpl(
      const AmbientAnimationStaticResourcesImpl&) = delete;
  AmbientAnimationStaticResourcesImpl& operator=(
      const AmbientAnimationStaticResourcesImpl&) = delete;

  ~AmbientAnimationStaticResourcesImpl() override = default;

  base::StringPiece GetLottieData() const override {
    base::StringPiece animation_json =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            lottie_json_resource_id_);
    DCHECK(!animation_json.empty());
    return animation_json;
  }

  gfx::ImageSkia GetStaticImageAsset(
      base::StringPiece asset_id) const override {
    if (!asset_id_to_resource_id_.contains(asset_id))
      return gfx::ImageSkia();

    const gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            asset_id_to_resource_id_.at(asset_id));
    DCHECK(image) << asset_id;
    return *image;
  }

 private:
  // Resource id for this animation theme's Lottie json data.
  const int lottie_json_resource_id_;
  // Map of all static image assets in this animation to their corresponding
  // resource ids. Points to global memory with static duration.
  const base::flat_map<base::StringPiece, int> asset_id_to_resource_id_;
};

}  // namespace

// static
std::unique_ptr<AmbientAnimationStaticResources>
AmbientAnimationStaticResources::Create(AmbientAnimationTheme theme) {
  if (!GetAnimationThemeToLottieResourceIdMap().contains(theme))
    return nullptr;

  return std::make_unique<AmbientAnimationStaticResourcesImpl>(
      GetAnimationThemeToLottieResourceIdMap().at(theme),
      GetAssetIdToResourceIdMapForTheme(theme));
}

}  // namespace ash
