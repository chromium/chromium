// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_config.h"

#include "ash/ambient/util/ambient_util.h"
#include "ash/utility/lottie_util.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace ash {
namespace {

void ParseDynamicAssetsIdsInAnimation(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata,
    std::size_t& num_total_positions_out,
    std::size_t& num_assets_per_position_out) {
  base::flat_map<std::string, std::size_t> position_to_num_assets;
  ambient::util::ParsedDynamicAssetId parsed_asset_id;
  for (const auto& [asset_id, _] : skottie_resource_metadata.asset_storage()) {
    if (!IsCustomizableLottieId(asset_id)) {
      DVLOG(4) << "Ignoring static image asset id";
      continue;
    }

    if (!ambient::util::ParseDynamicLottieAssetId(asset_id, parsed_asset_id)) {
      NOTREACHED() << "Lottie file contains invalid dynamic asset id "
                   << asset_id;
    }

    auto iter =
        position_to_num_assets
            .try_emplace(parsed_asset_id.position_id, /*initial count*/ 0)
            .first;
    ++iter->second;
  }

  if (position_to_num_assets.empty()) {
    num_total_positions_out = 0;
    num_assets_per_position_out = 0;
    return;
  }

  // Currently, it's expected that all positions in the animations have the same
  // number of assets assigned to it. If this fails, the animation is invalid
  // and must be updated by the designer as the rest of the pipeline was not
  // designed with case in mind.
  num_total_positions_out = position_to_num_assets.size();
  num_assets_per_position_out = position_to_num_assets.begin()->second;
  for (const auto& [position, num_assets_assigned] : position_to_num_assets) {
    if (num_assets_assigned != num_assets_per_position_out) {
      LOG(FATAL) << "Position " << position << " has " << num_assets_assigned
                 << "assets. Expected " << num_assets_per_position_out;
    }
  }
}

}  // namespace

ASH_EXPORT AmbientPhotoConfig CreateAmbientAnimationPhotoConfig(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata) {
  AmbientPhotoConfig config;
  config.should_split_topics = true;

  // Example: If there are 6 positions and 2 assets per position, this will
  // initially prepare and buffer 6 * 2 = 12 topics. Afterwards, all future
  // refreshes will prepare 6 topics, effectively giving each position a new
  // topic.
  ParseDynamicAssetsIdsInAnimation(
      skottie_resource_metadata,
      /*num_total_positions_out=*/config.topic_set_size,
      /*num_assets_per_position_out=*/config.num_topic_sets_to_buffer);
  // In the worst case scenario, the same topic can be replicated across all
  // of the assets in the animation. The screen will not burn due to the jitter
  // applied to the animation.
  config.min_total_topics_required = 1;

  // Once an animation cycle starts rendering (including the very first
  // cycle), start preparing the next set of decoded topics for the next
  // cycle. Unlike the slideshow view, this view waits until a new animation
  // cycle starts, then pulls the most recent topics from the model at that
  // time.
  config.refresh_topic_markers = {AmbientPhotoConfig::Marker::kUiStartRendering,
                                  AmbientPhotoConfig::Marker::kUiCycleEnded};
  return config;
}

}  // namespace ash
