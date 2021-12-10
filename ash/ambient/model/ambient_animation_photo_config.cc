// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_config.h"

#include "ash/ambient/util/ambient_util.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace ash {
namespace {

size_t GetNumDynamicAssetsInAnimation(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata) {
  size_t num_dynamic_assets = 0;
  for (const auto& resource_pair : skottie_resource_metadata.asset_storage()) {
    const std::string& asset_id = resource_pair.first;
    if (ambient::util::IsDynamicLottieAsset(asset_id))
      ++num_dynamic_assets;
  }
  return num_dynamic_assets;
}

}  // namespace

ASH_EXPORT AmbientPhotoConfig CreateAmbientAnimationPhotoConfig(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata) {
  return {
      // Unlike the full screen slideshow screensaver, the animated screensaver
      // has much smaller assets, and/so primary/related photos are never split
      // within the same asset and are assigned to separate ones.
      /*should_split_topics=*/true,
      // Unlike the slideshow screensaver, the animated screensaver has
      // motion/activity in it. So in the worst case scenario, we can repeat the
      // animation cycle with the same set of image assets indefinitely and the
      // screen won't burn. Hence, only 1 set of assets is required in the
      // buffer.
      /*num_decoded_topics_to_buffer=*/
      GetNumDynamicAssetsInAnimation(skottie_resource_metadata)};
}

}  // namespace ash
