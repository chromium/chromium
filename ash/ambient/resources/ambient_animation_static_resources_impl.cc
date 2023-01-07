// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/ambient/resources/grit/ash_ambient_lottie_resources.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "cc/paint/skottie_wrapper.h"
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
            IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_ANIMATION_JSON},
           {AmbientAnimationTheme::kFloatOnBy,
            IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_ANIMATION_JSON}});
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
              {ambient::resources::kClipBottomAssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_CLIP_BOTTOM_PNG},
              {ambient::resources::kClipTopAssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_CLIP_TOP_PNG},
              {ambient::resources::kFrameImage1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_FRAME_IMAGE_1_PNG},
              {ambient::resources::kFrameImage2AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_FRAME_IMAGE_2_PNG},
              {ambient::resources::kTreeShadowAssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_TREE_SHADOW_PNG},
              {ambient::resources::kStringAssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_STRING_PNG},
              // End Assets
          }
          // End Theme: Feel the Breeze
      },
      {
          // Theme: Float on By
          AmbientAnimationTheme::kFloatOnBy,
          {
              // Assets
              {ambient::resources::kShadowA1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_A_1_PNG},
              {ambient::resources::kShadowB1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_B_1_PNG},
              {ambient::resources::kShadowC1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_C_1_PNG},
              {ambient::resources::kShadowD1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_D_1_PNG},
              {ambient::resources::kShadowE1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_E_1_PNG},
              {ambient::resources::kShadowF1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_F_1_PNG},
              {ambient::resources::kShadowG1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_G_1_PNG},
              {ambient::resources::kShadowH1AssetId,
               IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_SHADOW_H_1_PNG},
              // End Assets
          }
          // End Theme: Float on By
      }
      // End Themes
  };
  DCHECK(m.contains(theme))
      << "Asset/resource ids missing for " << ToString(theme);
  return m.at(theme);
}

scoped_refptr<cc::SkottieWrapper> CreateSkottieWrapper(
    int lottie_json_resource_id,
    bool serializable) {
  base::StringPiece animation_json =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          lottie_json_resource_id);
  DCHECK(!animation_json.empty());
  base::span<const uint8_t> lottie_data_bytes =
      base::as_bytes(base::make_span(animation_json));
  scoped_refptr<cc::SkottieWrapper> animation;
  if (serializable) {
    // Create a serializable SkottieWrapper since the SkottieWrapper may have to
    // be serialized and transmitted over IPC for out-of-process rasterization.
    animation = cc::SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
        lottie_data_bytes.begin(), lottie_data_bytes.end()));
  } else {
    animation = cc::SkottieWrapper::CreateNonSerializable(lottie_data_bytes);
  }
  DCHECK(animation);
  DCHECK(animation->is_valid());
  return animation;
}

class AmbientAnimationStaticResourcesImpl
    : public AmbientAnimationStaticResources {
 public:
  AmbientAnimationStaticResourcesImpl(
      AmbientAnimationTheme theme,
      int lottie_json_resource_id,
      base::flat_map<base::StringPiece, int> asset_id_to_resource_id,
      bool create_serializable_skottie)
      : theme_(theme),
        animation_(CreateSkottieWrapper(lottie_json_resource_id,
                                        create_serializable_skottie)),
        asset_id_to_resource_id_(std::move(asset_id_to_resource_id)) {
    DCHECK(animation_);
  }

  AmbientAnimationStaticResourcesImpl(
      const AmbientAnimationStaticResourcesImpl&) = delete;
  AmbientAnimationStaticResourcesImpl& operator=(
      const AmbientAnimationStaticResourcesImpl&) = delete;

  ~AmbientAnimationStaticResourcesImpl() override = default;

  const scoped_refptr<cc::SkottieWrapper>& GetSkottieWrapper() const override {
    return animation_;
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

  AmbientAnimationTheme GetAmbientAnimationTheme() const override {
    return theme_;
  }

 private:
  const AmbientAnimationTheme theme_;
  // The skottie animation object built off of the animation json string
  // loaded from the resource pak.
  const scoped_refptr<cc::SkottieWrapper> animation_;
  // Map of all static image assets in this animation to their corresponding
  // resource ids. Points to global memory with static duration.
  const base::flat_map<base::StringPiece, int> asset_id_to_resource_id_;
};

}  // namespace

// static
std::unique_ptr<AmbientAnimationStaticResources>
AmbientAnimationStaticResources::Create(AmbientAnimationTheme theme,
                                        bool serializable) {
  if (!GetAnimationThemeToLottieResourceIdMap().contains(theme))
    return nullptr;

  return std::make_unique<AmbientAnimationStaticResourcesImpl>(
      theme, GetAnimationThemeToLottieResourceIdMap().at(theme),
      GetAssetIdToResourceIdMapForTheme(theme), serializable);
}

}  // namespace ash
