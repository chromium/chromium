// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_config.h"

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/util/ambient_util.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace ash {
namespace {

// Unlike the slideshow screensaver, the animated screensaver has
// motion/activity in it. So in the worst case scenario, we can repeat the
// animation cycle with the same set of image assets indefinitely and the screen
// won't burn. Hence, only 1 set of assets is required in the buffer.
constexpr int kNumSetsToBuffer = 1;

int GetNumDynamicAssetsInAnimation(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata) {
  int num_dynamic_assets = 0;
  for (const auto& resource_pair : skottie_resource_metadata.asset_storage()) {
    const std::string& asset_id = resource_pair.first;
    if (ambient::util::IsDynamicLottieAsset(asset_id))
      ++num_dynamic_assets;
  }
  return num_dynamic_assets;
}

}  // namespace

AmbientAnimationPhotoConfig::AmbientAnimationPhotoConfig(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata)
    : num_dynamic_assets_(
          GetNumDynamicAssetsInAnimation(skottie_resource_metadata)) {}

AmbientAnimationPhotoConfig::~AmbientAnimationPhotoConfig() = default;

int AmbientAnimationPhotoConfig::GetNumAssets() const {
  return num_dynamic_assets_;
}

int AmbientAnimationPhotoConfig::GetNumSetsOfAssetsToBuffer() const {
  return kNumSetsToBuffer;
}

int AmbientAnimationPhotoConfig::GetNumAssetsInTopic(
    const PhotoWithDetails& decoded_topic) const {
  // Unlike the full screen slideshow screensaver, the animated screensaver has
  // much smaller assets, and/so primary/related photos are never split within
  // the same asset and are assigned to separate ones.
  int num_assets_in_topic = 0;
  if (!decoded_topic.photo.isNull())
    ++num_assets_in_topic;
  if (!decoded_topic.related_photo.isNull())
    ++num_assets_in_topic;
  return num_assets_in_topic;
}

}  // namespace ash
