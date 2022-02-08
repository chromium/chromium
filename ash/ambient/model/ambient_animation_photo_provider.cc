// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_provider.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/utility/cropping_util.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_frame_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
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
                       const AmbientAnimationStaticResources& static_resources)
      : image_(static_resources.GetStaticImageAsset(asset_id)) {
    DCHECK(!ambient::util::IsDynamicLottieAsset(asset_id));
    DCHECK(!image_.isNull())
        << "Static image asset " << asset_id << " is unknown.";
    DVLOG(1) << "Loaded static asset " << asset_id;
  }

  cc::SkottieFrameData GetFrameData(float t, float scale_factor) override {
    if (!current_frame_data_.image ||
        current_frame_data_scale_factor_ != scale_factor) {
      current_frame_data_ = BuildSkottieFrameData(image_, scale_factor);
      current_frame_data_scale_factor_ = scale_factor;
    }
    return current_frame_data_;
  }

 private:
  // Private destructor since cc::SkottieFrameDataProvider::ImageAsset is a
  // ref-counted API.
  ~StaticImageAssetImpl() override = default;

  const gfx::ImageSkia image_;
  cc::SkottieFrameData current_frame_data_;
  float current_frame_data_scale_factor_ = 0;
};

// Provides images for dynamic assets based on the following UX requirements:
// * Make a best effort to assign portrait images to portrait assets and same
//   for landscape.
// * If there are less topics available than the number of dynamic assets in
//   the animation, the available photos should be evenly distributed and
//   duplicated among the assets. For example, if there are 2 topics available
//   and 6 dynamic assets, each topic should appear 3 times.
// * The photos should be shuffled among the assets between animation cycles.
class DynamicImageProvider {
 public:
  explicit DynamicImageProvider(
      const base::circular_deque<PhotoWithDetails>& all_available_topics) {
    DCHECK(!all_available_topics.empty())
        << "Animation should not have started rendering without any decoded "
           "photos in the model.";
    TopicReferenceVector all_available_topics_shuffled =
        ShuffleTopics(all_available_topics);
    for (auto& topic_ref : all_available_topics_shuffled) {
      // Note the AmbientPhotoConfig for animations states that topics from IMAX
      // containing primary and related photos should be split into 2. So the
      // related photo should always be null (hence no point in reading it).
      DCHECK(!topic_ref.get().photo.isNull());
      if (IsPortrait(topic_ref.get().photo.size())) {
        portrait_set_.topics.push_back(std::move(topic_ref));
      } else {
        landscape_set_.topics.push_back(std::move(topic_ref));
      }
    }
  }

  gfx::ImageSkia GetImageForAssetSize(
      const absl::optional<gfx::Size>& asset_size) {
    gfx::ImageSkia image;
    // If the |asset_size| is unavailable, this is unexpected but not fatal. The
    // choice to default to portrait is arbitrary.
    if (!asset_size || IsPortrait(*asset_size)) {
      image = GetNextImage(/*primary_topic_set=*/portrait_set_,
                           /*secondary_topic_set=*/landscape_set_);
    } else {
      image = GetNextImage(/*primary_topic_set=*/landscape_set_,
                           /*secondary_topic_set=*/portrait_set_);
    }
    DCHECK(!image.isNull());
    TryResetCurrentTopicIndices();
    return image;
  }

 private:
  using TopicReferenceVector =
      std::vector<std::reference_wrapper<const PhotoWithDetails>>;

  struct TopicSet {
    // Not mutated after DynamicImageProvider's constructor.
    TopicReferenceVector topics;
    // Incremented each time a topic is picked from the set and loops back to
    // 0 when all topics from all TopicSets have been exhausted.
    size_t current_topic_idx = 0;
  };

  static TopicReferenceVector ShuffleTopics(
      const base::circular_deque<PhotoWithDetails>& all_available_topics) {
    TopicReferenceVector topics_shuffled;
    for (const PhotoWithDetails& topic : all_available_topics) {
      topics_shuffled.push_back(std::cref(topic));
    }
    base::RandomShuffle(topics_shuffled.begin(), topics_shuffled.end());
    return topics_shuffled;
  }

  static bool IsPortrait(const gfx::Size& size) {
    DCHECK(!size.IsEmpty());
    return size.height() > size.width();
  }

  static gfx::ImageSkia GetNextImageFromTopicSet(TopicSet& topic_set) {
    if (topic_set.current_topic_idx >= topic_set.topics.size())
      return gfx::ImageSkia();

    gfx::ImageSkia image =
        topic_set.topics[topic_set.current_topic_idx].get().photo;
    ++topic_set.current_topic_idx;
    return image;
  }

  static gfx::ImageSkia GetNextImage(TopicSet& primary_topic_set,
                                     TopicSet& secondary_topic_set) {
    gfx::ImageSkia image = GetNextImageFromTopicSet(primary_topic_set);
    return image.isNull() ? GetNextImageFromTopicSet(secondary_topic_set)
                          : image;
  }

  void TryResetCurrentTopicIndices() {
    // Once all available topics have been exhausted, reset the
    // |current_topic_idx| for each TopicSet to start "fresh" again.
    if (landscape_set_.current_topic_idx >= landscape_set_.topics.size() &&
        portrait_set_.current_topic_idx >= portrait_set_.topics.size()) {
      landscape_set_.current_topic_idx = 0;
      portrait_set_.current_topic_idx = 0;
    }
  }

  TopicSet landscape_set_;
  TopicSet portrait_set_;
};

}  // namespace

class AmbientAnimationPhotoProvider::DynamicImageAssetImpl
    : public cc::SkottieFrameDataProvider::ImageAsset {
 public:
  // |refresh_image_cb| is invoked whenever an asset detects a new animation
  // cycle has started and it doesn't have a new image assigned to it yet. In
  // practice, there may be multiple dynamic assets in an animation. So the
  // first asset that detects a new animation cycle (which is arbitrary), will
  // trigger a refresh and all of the dynamic assets will be assigned a new
  // image when the callback is run. That is to say, for each animation cycle,
  // the refresh callback will be run exactly once regardless of the number of
  // frames in a cycle or dynamic assets in the animation.
  DynamicImageAssetImpl(base::StringPiece asset_id,
                        absl::optional<gfx::Size> size,
                        base::RepeatingClosure refresh_image_cb)
      : asset_id_(asset_id),
        size_(std::move(size)),
        refresh_image_cb_(std::move(refresh_image_cb)) {
    DCHECK(refresh_image_cb_);
    if (!size_)
      DLOG(ERROR) << "Dimensions unavailable for dynamic asset " << asset_id_;
  }

  void AssignNewImage(gfx::ImageSkia image) {
    DCHECK(!image.isNull());
    new_image_ = std::move(image);
  }

  cc::SkottieFrameData GetFrameData(float t, float scale_factor) override {
    DVLOG(4) << "GetFrameData for asset " << asset_id_ << " time " << t;
    bool is_first_rendered_frame =
        last_observed_animation_timestamp_ == kAnimationTimestampInvalid;
    // The animation frame timestamp units are dictated by Skottie and are
    // irrelevant here. The timestamp for each individual asset is monotonically
    // increasing until the animation loops back to the beginning, indicating
    // the start of a new cycle.
    bool is_starting_new_cycle = t < last_observed_animation_timestamp_;
    last_observed_animation_timestamp_ = t;
    if (!is_first_rendered_frame && !is_starting_new_cycle) {
      DVLOG(4) << "No update required to dynamic asset at this time";
      return GetCurrentFrameData(scale_factor);
    }

    if (new_image_.isNull())
      refresh_image_cb_.Run();

    DCHECK(!new_image_.isNull());
    current_frame_data_ = BuildSkottieFrameData(new_image_, scale_factor);
    current_frame_data_scale_factor_ = scale_factor;
    new_image_ = gfx::ImageSkia();
    DVLOG(4) << "Returning new image for dynamic asset " << asset_id_;
    return current_frame_data_;
  }

  const absl::optional<gfx::Size>& size() const { return size_; }

  const std::string& asset_id() const { return asset_id_; }

 private:
  static constexpr float kAnimationTimestampInvalid = -1.f;

  // Private destructor since cc::SkottieFrameDataProvider::ImageAsset is a
  // ref-counted API.
  ~DynamicImageAssetImpl() override = default;

  const cc::SkottieFrameData& GetCurrentFrameData(float scale_factor) {
    DCHECK(current_frame_data_.image);
    if (current_frame_data_scale_factor_ != scale_factor) {
      current_frame_data_ = BuildSkottieFrameData(new_image_, scale_factor);
      current_frame_data_scale_factor_ = scale_factor;
    }
    return current_frame_data_;
  }

  const std::string asset_id_;
  const absl::optional<gfx::Size> size_;
  const base::RepeatingClosure refresh_image_cb_;
  // Last animation frame timestamp that was observed.
  float last_observed_animation_timestamp_ = kAnimationTimestampInvalid;
  gfx::ImageSkia new_image_;
  cc::SkottieFrameData current_frame_data_;
  float current_frame_data_scale_factor_ = 0;
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
    const base::FilePath& resource_path,
    const absl::optional<gfx::Size>& size) {
  // Note in practice, all of the image assets are loaded one time by Skottie
  // when the animation is initially loaded. So the set of assets does not
  // change once the animation starts rendering.
  if (ambient::util::IsDynamicLottieAsset(asset_id)) {
    dynamic_assets_.push_back(base::MakeRefCounted<DynamicImageAssetImpl>(
        asset_id, size,
        base::BindRepeating(
            &AmbientAnimationPhotoProvider::RefreshDynamicImageAssets,
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
    // For static assets, the |size| isn't needed. It should match the size of
    // the image loaded from animation's |static_resources_| since that is the
    // very image created by UX when the animation was built.
    return base::MakeRefCounted<StaticImageAssetImpl>(asset_id,
                                                      *static_resources_);
  }
}

void AmbientAnimationPhotoProvider::RefreshDynamicImageAssets() {
  DVLOG(4) << __func__;
  DynamicImageProvider image_provider(backend_model_->all_decoded_topics());
  for (const auto& dynamic_asset : dynamic_assets_) {
    gfx::ImageSkia assigned_image =
        image_provider.GetImageForAssetSize(dynamic_asset->size());
    if (dynamic_asset->size()) {
      DCHECK(assigned_image.bitmap());
      // Crop the image such that it exactly matches the aspect ratio of the
      // asset that it's assigned to. Skottie will handle rescaling the image to
      // the desired ultimate dimensions farther down the pipeline.
      assigned_image = gfx::ImageSkia::CreateFrom1xBitmap(
          CenterCropImage(*assigned_image.bitmap(), *dynamic_asset->size()));
    } else {
      DLOG(ERROR) << "Dynamic asset " << dynamic_asset->asset_id()
                  << " missing dimensions in lottie file";
    }
    dynamic_asset->AssignNewImage(std::move(assigned_image));
  }
}

}  // namespace ash
