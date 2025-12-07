// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/ambient/resources/grit/ash_ambient_lottie_resources.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

using ash::personalization_app::mojom::AmbientTheme;
using AmbientThemeToResourceIdMap = base::flat_map<AmbientTheme, int>;
using AssetIdToResourceIdMap = base::flat_map<std::string_view, int>;

const AmbientThemeToResourceIdMap& GetAmbientThemeToLottieResourceIdMap() {
  static const AmbientThemeToResourceIdMap* m = new AmbientThemeToResourceIdMap(
      {{AmbientTheme::kFeelTheBreeze,
        IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FEEL_THE_BREEZE_ANIMATION_JSON},
       {AmbientTheme::kFloatOnBy,
        IDR_ASH_AMBIENT_LOTTIE_LOTTIE_FLOAT_ON_BY_ANIMATION_JSON}});
  return *m;
}

// TODO(esum): Look into auto-generating this map and the one above via a
// build-time script.
AssetIdToResourceIdMap GetAssetIdToResourceIdMapForTheme(AmbientTheme theme) {
  base::flat_map<AmbientTheme, AssetIdToResourceIdMap> m = {
      // Themes
      {
          // Theme: Feel the Breeze
          AmbientTheme::kFeelTheBreeze,
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
          AmbientTheme::kFloatOnBy,
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
  DCHECK(m.contains(theme)) << "Asset/resource ids missing for "
                            << ambient::util::AmbientThemeToString(theme);
  return m.at(theme);
}

scoped_refptr<cc::SkottieWrapper> CreateSkottieWrapper(
    int lottie_json_resource_id,
    bool serializable) {
  std::string animation_json =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          lottie_json_resource_id);
  DCHECK(!animation_json.empty());
  base::span<const uint8_t> lottie_data_bytes =
      base::as_byte_span(animation_json);
  scoped_refptr<cc::SkottieWrapper> animation;
  if (serializable) {
    // Create a serializable SkottieWrapper since the SkottieWrapper may have to
    // be serialized and transmitted over IPC for out-of-process rasterization.
    animation =
        cc::SkottieWrapper::UnsafeCreateSerializable(std::vector<uint8_t>(
            lottie_data_bytes.begin(), lottie_data_bytes.end()));
  } else {
    animation =
        cc::SkottieWrapper::UnsafeCreateNonSerializable(lottie_data_bytes);
  }
  DCHECK(animation);
  DCHECK(animation->is_valid());
  return animation;
}

class AmbientAnimationStaticResourcesImpl
    : public AmbientAnimationStaticResources {
 public:
  AmbientAnimationStaticResourcesImpl(
      AmbientUiSettings ui_settings,
      int lottie_json_resource_id,
      base::flat_map<std::string_view, int> asset_id_to_resource_id,
      bool create_serializable_skottie)
      : ui_settings_(std::move(ui_settings)),
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

  gfx::ImageSkia GetStaticImageAsset(std::string_view asset_id) const override {
    if (!asset_id_to_resource_id_.contains(asset_id))
      return gfx::ImageSkia();

    const gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            asset_id_to_resource_id_.at(asset_id));
    DCHECK(image) << asset_id;
    return *image;
  }

  const AmbientUiSettings& GetUiSettings() const override {
    return ui_settings_;
  }

 private:
  const AmbientUiSettings ui_settings_;
  // The skottie animation object built off of the animation json string
  // loaded from the resource pak.
  const scoped_refptr<cc::SkottieWrapper> animation_;
  // Map of all static image assets in this animation to their corresponding
  // resource ids. Points to global memory with static duration.
  const base::flat_map<std::string_view, int> asset_id_to_resource_id_;
};

}  // namespace

// static
std::unique_ptr<AmbientAnimationStaticResources>
AmbientAnimationStaticResources::Create(AmbientUiSettings ui_settings,
                                        bool serializable) {
  if (!GetAmbientThemeToLottieResourceIdMap().contains(ui_settings.theme())) {
    return nullptr;
  }

  return std::make_unique<AmbientAnimationStaticResourcesImpl>(
      ui_settings,
      GetAmbientThemeToLottieResourceIdMap().at(ui_settings.theme()),
      GetAssetIdToResourceIdMapForTheme(ui_settings.theme()), serializable);
}

}  // namespace ash
