// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_provider.h"

#include <string>
#include <utility>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/util/ambient_util.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_frame_data.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

namespace {

cc::SkottieFrameData BuildSkottieFrameData(const gfx::ImageSkia& image,
                                           float scale_factor) {
  // TODO(esum): Experiment with different filter qualities for different asset
  // types. Thus far, "high" quality has a large impact on performance;
  // the frame rate is cut in half due to the increased computational
  // complexity. "Medium" quality is the best compromise so far with little to
  // no visible difference from "high" quality while maintaining close to 60
  // fps.
  static constexpr cc::PaintFlags::FilterQuality kFilterQuality =
      cc::PaintFlags::FilterQuality::kMedium;

  DCHECK(!image.isNull());
  const gfx::ImageSkiaRep& image_rep = image.GetRepresentation(scale_factor);
  DCHECK(!image_rep.is_null());
  DCHECK(image_rep.has_paint_image());
  return {
      /*image=*/image_rep.paint_image(),
      /*quality=*/kFilterQuality,
  };
}

class StaticImageAssetImpl : public cc::SkottieFrameDataProvider::ImageAsset {
 public:
  StaticImageAssetImpl(base::StringPiece asset_id,
                       const AmbientAnimationStaticResources* static_resources)
      : asset_id_(std::string(asset_id)), static_resources_(static_resources) {
    DCHECK(!ambient::util::IsDynamicLottieAsset(asset_id_));
    DCHECK(static_resources_);
  }

  absl::optional<cc::SkottieFrameData> GetFrameData(float t, float scale_factor)
      override {
    // The static image only needs to be provided one time in the animation's
    // lifetime. Afterwards, return nullopt to indicate no change to the asset's
    // contents.
    if (has_provided_first_frame_)
      return absl::nullopt;

    has_provided_first_frame_ = true;
    gfx::ImageSkia image = static_resources_->GetStaticImageAsset(asset_id_);
    DCHECK(!image.isNull())
        << "Static image asset " << asset_id_ << " is unknown.";
    return BuildSkottieFrameData(image, scale_factor);
  }

 private:
  // Private destructor since cc::SkottieFrameDataProvider::ImageAsset is a
  // ref-counted API.
  ~StaticImageAssetImpl() override = default;

  const std::string asset_id_;
  const AmbientAnimationStaticResources* const static_resources_;
  bool has_provided_first_frame_ = false;
};

}  // namespace

class AmbientAnimationPhotoProvider::DynamicImageAssetImpl
    : public cc::SkottieFrameDataProvider::ImageAsset {
 public:
  using AnimationTimestampCallback =
      base::RepeatingCallback<void(float new_timestamp)>;

  explicit DynamicImageAssetImpl(AnimationTimestampCallback timestamp_cb)
      : timestamp_cb_(std::move(timestamp_cb)) {
    DCHECK(timestamp_cb_);
  }

  void AssignNewImage(gfx::ImageSkia image) {
    DCHECK(!image.isNull());
    new_image_ = std::move(image);
  }

  absl::optional<cc::SkottieFrameData> GetFrameData(float t, float scale_factor)
      override {
    // Run the callback first with the new animation timestamp to see if the
    // provider will assign this asset a new image.
    timestamp_cb_.Run(t);
    // In practice, |new_image_| will only be non-null at the start of each
    // animation cycle. For all other frames, return nullopt (no update).
    if (new_image_.isNull())
      return absl::nullopt;

    cc::SkottieFrameData frame_data =
        BuildSkottieFrameData(new_image_, scale_factor);
    new_image_ = gfx::ImageSkia();
    return frame_data;
  }

 private:
  // Private destructor since cc::SkottieFrameDataProvider::ImageAsset is a
  // ref-counted API.
  ~DynamicImageAssetImpl() override = default;

  const AnimationTimestampCallback timestamp_cb_;
  gfx::ImageSkia new_image_;
};

AmbientAnimationPhotoProvider::AmbientAnimationPhotoProvider(
    const AmbientAnimationStaticResources* static_resources,
    const AmbientBackendModel* backend_model)
    : static_resources_(static_resources),
      backend_model_(backend_model),
      weak_factory_(this) {
  DCHECK(static_resources_);
  DCHECK(backend_model_);
}

AmbientAnimationPhotoProvider::~AmbientAnimationPhotoProvider() = default;

scoped_refptr<cc::SkottieFrameDataProvider::ImageAsset>
AmbientAnimationPhotoProvider::LoadImageAsset(
    base::StringPiece asset_id,
    const base::FilePath& resource_path) {
  // Note in practice, all of the image assets are loaded one time by Skottie
  // when the animation is initially loaded. So the set of assets does not
  // change once the animation starts rendering.
  if (ambient::util::IsDynamicLottieAsset(asset_id)) {
    dynamic_assets_.push_back(
        base::MakeRefCounted<DynamicImageAssetImpl>(base::BindRepeating(
            &AmbientAnimationPhotoProvider::OnAnimationTimestampUpdated,
            // In practice, this could be Unretained since the provider will
            // outlive the assets in the lottie::Animation class. But use a
            // WeakPtr here just to put the reader's mind at ease. If the
            // provider theoretically was destroyed before its assets, the code
            // wouldn't crash, and the assets just wouldn't receive further
            // photo refresh updates. Alternatively,
            // AmbientAnimationPhotoProvider could be made ref-counted, but that
            // is overkill to account for something that isn't an actual issue.
            weak_factory_.GetWeakPtr())));
    return dynamic_assets_.back();
  } else {
    return base::MakeRefCounted<StaticImageAssetImpl>(asset_id,
                                                      static_resources_);
  }
}

// If there are N dynamic assets in the animation, then for each rendered frame
// with timestamp T, OnAnimationTimestampUpdate(T) will be called N times. If a
// new animation cycle is detected on the first call to
// OnAnimationTimestampUpdate(T), the provider will pull a new set of images
// from the model and assign them to all of the dynamic assets. The subsequent
// N - 1 calls to OnAnimationTimestampUpdate(T) will be no-ops. That is to
// say, for each animation cycle, this method will only trigger a refresh one
// time regardless of the number of frames in a cycle or dynamic assets in the
// animation.
void AmbientAnimationPhotoProvider::OnAnimationTimestampUpdated(
    float new_timestamp) {
  bool is_first_rendered_frame =
      last_observed_animation_timestamp_ == kAnimationTimestampInvalid;
  // The animation frame timestamp units are dictated by Skottie and are
  // irrelevant here. The timestamp is monotonically increasing until the
  // animation loops back to the beginning, indicating the start of a new cycle.
  bool is_starting_new_cycle =
      new_timestamp < last_observed_animation_timestamp_;
  last_observed_animation_timestamp_ = new_timestamp;
  if (!is_first_rendered_frame && !is_starting_new_cycle) {
    DVLOG(4) << "No update required to dynamic assets at this time";
    return;
  }

  const base::circular_deque<PhotoWithDetails>& all_available_topics =
      backend_model_->all_decoded_topics();
  DCHECK(!all_available_topics.empty())
      << "Animation should not have started rendering without any decoded "
         "photos in the model.";
  // UX requirements:
  // 1) If there are less topics available than the number of dynamic assets in
  //    the animation, the available photos should be evenly distributed and
  //    duplicated among the assets. For example, if there are 2 topics
  //    available and 6 dynamic assets, each topic should appear 3 times.
  // 2) The photos should be shuffled among the assets between animation cycles.
  std::vector<gfx::ImageSkia> assigned_images(dynamic_assets_.size());
  size_t decoded_topic_idx = 0;
  for (gfx::ImageSkia& image_to_assign : assigned_images) {
    DCHECK(!all_available_topics[decoded_topic_idx].photo.isNull());
    // Note the AmbientPhotoConfig for animations states that topics from IMAX
    // containing primary and related photos should be split into 2. So the
    // related photo should always be null (hence no point in reading it here).
    image_to_assign = all_available_topics[decoded_topic_idx].photo;
    decoded_topic_idx = (decoded_topic_idx + 1) % all_available_topics.size();
  }

  base::RandomShuffle(assigned_images.begin(), assigned_images.end());
  for (size_t asset_idx = 0; asset_idx < dynamic_assets_.size(); ++asset_idx) {
    dynamic_assets_[asset_idx]->AssignNewImage(assigned_images[asset_idx]);
  }
}

}  // namespace ash
