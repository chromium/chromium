// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_provider.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/utility/cropping_util.h"
#include "ash/utility/lottie_util.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_frame_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

namespace {

// TODO(esum): Experiment with different filter qualities for different asset
// types. Thus far, "high" quality has a large impact on performance;
// the frame rate is cut in half due to the increased computational
// complexity. "Medium" quality is the best compromise so far with little to
// no visible difference from "high" quality while maintaining close to 60
// fps.
constexpr cc::PaintFlags::FilterQuality kFilterQuality =
    cc::PaintFlags::FilterQuality::kMedium;

cc::SkottieFrameData BuildSkottieFrameData(const gfx::ImageSkia& image,
                                           float scale_factor) {
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
    DCHECK(!IsCustomizableLottieId(asset_id));
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

  const PhotoWithDetails& GetTopicForAssetSize(
      const absl::optional<gfx::Size>& asset_size) {
    const PhotoWithDetails* topic = nullptr;
    // If the |asset_size| is unavailable, this is unexpected but not fatal. The
    // choice to default to portrait is arbitrary.
    if (!asset_size || IsPortrait(*asset_size)) {
      topic = GetNextTopic(/*primary_topic_set=*/portrait_set_,
                           /*secondary_topic_set=*/landscape_set_);
    } else {
      topic = GetNextTopic(/*primary_topic_set=*/landscape_set_,
                           /*secondary_topic_set=*/portrait_set_);
    }
    DCHECK(topic);
    TryResetCurrentTopicIndices();
    return *topic;
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

  static const PhotoWithDetails* GetNextTopicFromTopicSet(TopicSet& topic_set) {
    if (topic_set.current_topic_idx >= topic_set.topics.size())
      return nullptr;

    const PhotoWithDetails* topic =
        &topic_set.topics[topic_set.current_topic_idx].get();
    ++topic_set.current_topic_idx;
    return topic;
  }

  static const PhotoWithDetails* GetNextTopic(TopicSet& primary_topic_set,
                                              TopicSet& secondary_topic_set) {
    const PhotoWithDetails* topic = GetNextTopicFromTopicSet(primary_topic_set);
    return topic ? topic : GetNextTopicFromTopicSet(secondary_topic_set);
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
  DynamicImageAssetImpl(
      base::StringPiece asset_id,
      absl::optional<gfx::Size> size,
      const base::WeakPtr<AmbientAnimationPhotoProvider>& provider)
      : asset_id_(asset_id), size_(std::move(size)), provider_(provider) {
    DCHECK(provider_);
    if (!size_)
      DLOG(ERROR) << "Dimensions unavailable for dynamic asset " << asset_id_;
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
    if (is_first_rendered_frame || is_starting_new_cycle) {
      DVLOG(4) << "Returning new image for dynamic asset " << asset_id_;
      if (provider_) {
        current_topic_ = provider_->GenerateNextTopicForDynamicAsset(*this);
        // Force |current_frame_data_| to be reset below.
        current_frame_data_scale_factor_ = kImageScaleFactorInvalid;
      } else {
        // If this corner case does somehow happen, it will only be for a brief
        // period when the animation is being torn down.
        DVLOG(1) << "AmbientAnimationPhotoProvider has been destroyed. Cannot "
                    "refresh images.";
      }
    } else {
      DVLOG(4) << "No update required to dynamic asset at this time";
    }
    SetCurrentFrameDataForScale(scale_factor);
    return current_frame_data_;
  }

  const absl::optional<gfx::Size>& size() const { return size_; }

  const std::string& asset_id() const { return asset_id_; }

 private:
  static constexpr float kAnimationTimestampInvalid = -1.f;
  static constexpr float kImageScaleFactorInvalid = 0.f;

  // Private destructor since cc::SkottieFrameDataProvider::ImageAsset is a
  // ref-counted API.
  ~DynamicImageAssetImpl() override = default;

  void SetCurrentFrameDataForScale(float scale_factor) {
    static constexpr float kScaleFactorEpsilon = 0.01f;
    DCHECK(!current_topic_.photo.isNull());
    if (current_frame_data_scale_factor_ != kImageScaleFactorInvalid &&
        base::IsApproximatelyEqual(current_frame_data_scale_factor_,
                                   scale_factor, kScaleFactorEpsilon)) {
      DVLOG(4) << "Current frame data already matches target scale.";
      return;
    }

    // First load the closest image representation from the source at the target
    // |scale_factor|, then crop the image representation to the asset's aspect
    // ratio.
    const gfx::ImageSkiaRep& image_rep =
        current_topic_.photo.GetRepresentation(scale_factor);
    DCHECK(!image_rep.is_null());
    cc::PaintImage paint_image;
    if (size_) {
      // Crop the image such that it exactly matches this asset's aspect ratio.
      // Skottie will handle rescaling the image to the exact desired
      // dimensions farther down the pipeline.
      SkBitmap cropped_bitmap = CenterCropImage(image_rep.GetBitmap(), *size_);
      // Prevents a deep copy in PaintImage::CreateFromBitmap().
      cropped_bitmap.setImmutable();
      paint_image = cc::PaintImage::CreateFromBitmap(std::move(cropped_bitmap));
    } else {
      DLOG(ERROR) << "Dynamic asset " << asset_id_
                  << " missing dimensions in lottie file";
      DCHECK(image_rep.has_paint_image());
      paint_image = image_rep.paint_image();
    }
    current_frame_data_.image = std::move(paint_image);
    current_frame_data_.quality = kFilterQuality;
    current_frame_data_scale_factor_ = scale_factor;
  }

  const std::string asset_id_;
  const absl::optional<gfx::Size> size_;
  const base::WeakPtr<AmbientAnimationPhotoProvider> provider_;
  // Last animation frame timestamp that was observed.
  float last_observed_animation_timestamp_ = kAnimationTimestampInvalid;
  gfx::ImageSkia new_image_;
  cc::SkottieFrameData current_frame_data_;
  float current_frame_data_scale_factor_ = kImageScaleFactorInvalid;
  // The original topic off of which |current_frame_data_| was built. May have
  // multiple scale representations in its image in the event that a different
  // scale factor is required while rendering.
  PhotoWithDetails current_topic_;
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
  if (IsCustomizableLottieId(asset_id)) {
    dynamic_assets_.push_back(base::MakeRefCounted<DynamicImageAssetImpl>(
        asset_id, size, weak_factory_.GetWeakPtr()));
    return dynamic_assets_.back();
  } else {
    // For static assets, the |size| isn't needed. It should match the size of
    // the image loaded from animation's |static_resources_| since that is the
    // very image created by UX when the animation was built.
    return base::MakeRefCounted<StaticImageAssetImpl>(asset_id,
                                                      *static_resources_);
  }
}

void AmbientAnimationPhotoProvider::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void AmbientAnimationPhotoProvider::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

// Invoked whenever an asset detects a new animation cycle has started. In
// practice, there may be multiple dynamic assets in an animation. So the
// first asset that detects a new animation cycle (which is arbitrary), will
// cause the provider internally to find a new topic for *all* dynamic assets in
// the animation. The provider then returns the first asset's assigned topic and
// saves the other N - 1 assets' topics, marking them as pending. When the other
// N - 1 assets call GenerateNextTopicForDynamicAsset() shortly after, the
// provider simply retrieves the corresponding pending topic until the set of
// pending topics is empty. This process then repeats at the start of the next
// animation cycle.
PhotoWithDetails
AmbientAnimationPhotoProvider::GenerateNextTopicForDynamicAsset(
    const DynamicImageAssetImpl& target_asset) {
  DVLOG(4) << __func__;
  PhotoWithDetails topic_for_target_asset =
      ExtractPendingTopicForDynamicAsset(target_asset);
  if (!topic_for_target_asset.photo.isNull()) {
    return topic_for_target_asset;
  }

  DCHECK(pending_dynamic_asset_topics_.empty())
      << "All pending topics should have been returned before the first frame "
         "of each animation cycle.";
  DynamicImageProvider image_provider(backend_model_->all_decoded_topics());
  for (const auto& dynamic_asset : dynamic_assets_) {
    pending_dynamic_asset_topics_.emplace(
        dynamic_asset.get(),
        image_provider.GetTopicForAssetSize(dynamic_asset->size()));
  }
  NotifyObserverOfNewTopics();
  topic_for_target_asset = ExtractPendingTopicForDynamicAsset(target_asset);
  DCHECK(!topic_for_target_asset.photo.isNull())
      << "GenerateNextTopicForDynamicAsset() for unknown asset "
      << target_asset.asset_id();
  return topic_for_target_asset;
}

PhotoWithDetails
AmbientAnimationPhotoProvider::ExtractPendingTopicForDynamicAsset(
    const DynamicImageAssetImpl& asset) {
  auto pending_topic_iter = pending_dynamic_asset_topics_.find(&asset);
  if (pending_topic_iter == pending_dynamic_asset_topics_.end()) {
    return PhotoWithDetails();
  } else {
    PhotoWithDetails pending_topic = std::move(pending_topic_iter->second);
    pending_dynamic_asset_topics_.erase(pending_topic_iter);
    return pending_topic;
  }
}

void AmbientAnimationPhotoProvider::NotifyObserverOfNewTopics() {
  base::flat_map</*asset_id*/ std::string,
                 std::reference_wrapper<const PhotoWithDetails>>
      new_topics;
  for (const auto& [asset, topic] : pending_dynamic_asset_topics_) {
    new_topics.emplace(asset->asset_id(), std::cref(topic));
  }
  for (Observer& obs : observers_) {
    obs.OnDynamicImageAssetsRefreshed(new_topics);
  }
}

}  // namespace ash
