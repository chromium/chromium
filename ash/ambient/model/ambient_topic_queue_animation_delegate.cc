// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_topic_queue_animation_delegate.h"

#include <algorithm>

#include "ash/ambient/util/ambient_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "ui/gfx/geometry/size_f.h"

namespace ash {
namespace {

bool IsPortrait(const gfx::Size& size) {
  DCHECK(!size.IsEmpty());
  return size.height() > size.width();
}

bool IsSquare(const gfx::Size& size) {
  DCHECK(!size.IsEmpty());
  // This is arbitrary. Just a rough estimate that a "square" picture has an
  // aspect ratio in the range [1 - kAspectRatioDelta, 1 + kAspectRatioDelta].
  static constexpr float kAspectRatioDelta = 0.05f;
  static constexpr float kAspectRatioLowerBound = 1.f - kAspectRatioDelta;
  static constexpr float kAspectRatioUpperBound = 1.f + kAspectRatioDelta;
  float aspect_ratio = gfx::SizeF(size).AspectRatio();
  return aspect_ratio > kAspectRatioLowerBound &&
         aspect_ratio < kAspectRatioUpperBound;
}

// Determines one size that best represents the group of image assets in the
// |resource_metadata| whose orientation matches |is_portrait|. The logic is
// currently as follows:
// * Compute the average aspect ratio of all assets with matching orientation.
// * Calculate the smallest size whose a) aspect ratio matches the average
//   computed above and b) dimensions exceed those of all assets with matching
//   orientation. This ensures that we ultimately download the largest
//   possible resolution of photos from IMAX and any resizing that happens
//   "shrinks" the photo to fit in the animation, which generally has better
//   quality that "growing" a photo.
// * Discard any "square" orientations from the aspect ratio calculation. These
//   are outliers that aren't quite portrait or landscape and bias the average
//   aspect ratio. Since they are "square", it is a good enough compromise to
//   use either a portrait or landscape photo and center-crop it to a square
//   orientation before rendering. If this is not good enough in the future, we
//   can return a third size in |GetTopicSizes()|, but it is currently not worth
//   it.
//
// Returns an empty gfx::Size instance if there are no assets that match the
// |is_portrait| orientation.
gfx::Size SummarizeImageAssetSizes(
    const cc::SkottieResourceMetadataMap& resource_metadata,
    bool is_portrait) {
  constexpr int kDimensionInvalid = -1;
  int largest_width_observed = kDimensionInvalid;
  int largest_height_observed = kDimensionInvalid;
  float aspect_ratio_sum = 0.f;
  int num_assets_found = 0;
  for (const auto& [asset_id, asset_metadata] :
       resource_metadata.asset_storage()) {
    // IMAX photos are only assigned to dynamic image assets in the animation,
    // so static image assets should be ignored when calculating.
    ambient::util::ParsedDynamicAssetId parsed_dynamic_asset_id;
    bool is_dynamic_image_asset = ambient::util::ParseDynamicLottieAssetId(
        asset_id, parsed_dynamic_asset_id);
    if (!is_dynamic_image_asset || !asset_metadata.size.has_value() ||
        IsPortrait(*asset_metadata.size) != is_portrait) {
      continue;
    }

    largest_width_observed =
        std::max(asset_metadata.size->width(), largest_width_observed);
    largest_height_observed =
        std::max(asset_metadata.size->height(), largest_height_observed);
    if (!IsSquare(*asset_metadata.size)) {
      ++num_assets_found;
      aspect_ratio_sum += gfx::SizeF(*asset_metadata.size).AspectRatio();
    }
  }

  if (num_assets_found == 0) {
    if (largest_width_observed == kDimensionInvalid) {
      // There were no assets matching the desired orientation.
      return gfx::Size();
    } else {
      // There were assets matching the desired orientation, but all of them
      // were closer to being "square".
      int square_length =
          std::max(largest_width_observed, largest_height_observed);
      return gfx::Size(square_length, square_length);
    }
  }

  float average_aspect_ratio = aspect_ratio_sum / num_assets_found;
  // There are corner cases here where an asset found above may ultimately have
  // a dimension larger than the computed size, but it's not worth accounting
  // for.
  gfx::Size candidate_a = gfx::Size(
      largest_width_observed,
      base::ClampRound<int>(largest_width_observed / average_aspect_ratio));
  gfx::Size candidate_b = gfx::Size(
      base::ClampRound<int>(largest_height_observed * average_aspect_ratio),
      largest_height_observed);
  // Both candidates should have the same aspect ratio, so comparing one of the
  // dimensions (width in this case) is sufficient.
  return candidate_a.width() > candidate_b.width() ? candidate_a : candidate_b;
}

// The output will always have 1 size for landscape assets and 1 size for
// portrait assets (or 0 if there are no assets of a particular orientation).
std::vector<gfx::Size> ComputeTopicSizes(
    const cc::SkottieResourceMetadataMap& resource_metadata) {
  static constexpr gfx::Size kDefaultTopicSize = gfx::Size(500, 500);

  gfx::Size landscape_size =
      SummarizeImageAssetSizes(resource_metadata, /*is_portrait=*/false);
  gfx::Size portrait_size =
      SummarizeImageAssetSizes(resource_metadata, /*is_portrait=*/true);
  std::vector<gfx::Size> output;
  if (!landscape_size.IsEmpty())
    output.push_back(std::move(landscape_size));
  if (!portrait_size.IsEmpty())
    output.push_back(std::move(portrait_size));

  if (output.empty()) {
    LOG(DFATAL) << "Failed to compute topic sizes for animation. Animation "
                   "file is likely invalid.";
    return {kDefaultTopicSize};
  }
  return output;
}

}  // namespace

AmbientTopicQueueAnimationDelegate::AmbientTopicQueueAnimationDelegate(
    const cc::SkottieResourceMetadataMap& resource_metadata)
    : topic_sizes_(ComputeTopicSizes(resource_metadata)) {}

AmbientTopicQueueAnimationDelegate::~AmbientTopicQueueAnimationDelegate() =
    default;

std::vector<gfx::Size> AmbientTopicQueueAnimationDelegate::GetTopicSizes() {
  // At the time this was written, UX has agreed that the landscape and portrait
  // versions of a given animation theme will have the same image asset sizes
  // (only the animation's layout will be different). Thus, it is sufficient
  // and simplest to just compute the desired topic sizes once with whichever
  // version of the animation is loaded initially (either topic or landscape).
  //
  // If this changes in the future, this will need to recompute topic sizes with
  // the new animation orientation.
  return topic_sizes_;
}

}  // namespace ash
