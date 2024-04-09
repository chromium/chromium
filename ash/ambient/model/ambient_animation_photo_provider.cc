// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// How dynamic assets are handled:
//
// Terminology:
// "Position" - A physical location on the screen where a dynamic asset appears.
// Its identifier is arbitrary, opaque, and embedded in the dynamic asset's id.
//
// "Index"- There shall be 1 or more assets assigned to each position. For
// example, if an animation has a cross-fade transition from image 1 to image 2,
// there may be 2 dynamic assets in the animation that share the same position.
// However, their indices will be different. Example:
// "_CrOS_Photo_PositionA_1" (Index 1 Position A)
// "_CrOS_Photo_PositionA_2" (Index 2 Position A)
// ...
//
// Now we'll step through an example with 2 positions and 2 assets per position:
// "_CrOS_Photo_PositionA_1"
// "_CrOS_Photo_PositionA_2"
// "_CrOS_Photo_PositionB_1"
// "_CrOS_Photo_PositionB_2"
//
// On the very first frame, the provider assigns a topic to each asset in the
// model using all topics available in the model:
// "_CrOS_Photo_PositionA_1" -> "TopicA"
// "_CrOS_Photo_PositionA_2" -> "TopicB"
// "_CrOS_Photo_PositionB_1" -> "TopicC"
// "_CrOS_Photo_PositionB_2" -> "TopicD"
//
// At the start of each new animation cycle, the provider first "rotates" the
// topics at each position. The topic previously assigned to asset with index
// <i + 1> is now assigned to asset with index <i> for all <i> at a given
// position. After rotation, the asset with the highest index at each position
// is left without an assigned topic:
// "_CrOS_Photo_PositionA_1" -> "TopicB"
// "_CrOS_Photo_PositionA_2" -> ???
// "_CrOS_Photo_PositionB_1" -> "TopicD"
// "_CrOS_Photo_PositionB_2" -> ???
//
// The provider then pulls the latest 2 topics from the model (since there are
// 2 assets left now without an assigned topic), and assigns a new topic to
// those assets.
// "_CrOS_Photo_PositionA_1" -> "TopicB"
// "_CrOS_Photo_PositionA_2" -> "TopicE" (new)
// "_CrOS_Photo_PositionB_1" -> "TopicD"
// "_CrOS_Photo_PositionB_2" -> "TopicF" (new)
//
// The process above repeats for each new animation cycle. Note the process
// generalizes to the simplest case where there is only 1 assigned topic per
// position. Rotation will just leave all assets in the animation without an
// assigned topic.

#include "ash/ambient/model/ambient_animation_photo_provider.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/utility/cropping_util.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_frame_data.h"
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

bool IsPortrait(const gfx::Size& size) {
  DCHECK(!size.IsEmpty());
  return size.height() > size.width();
}

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
  using TopicReferenceVector =
      std::vector<std::reference_wrapper<const PhotoWithDetails>>;

  explicit DynamicImageProvider(TopicReferenceVector all_available_topics) {
    DCHECK(!all_available_topics.empty())
        << "Animation should not have started rendering without any decoded "
           "photos in the model.";
    base::RandomShuffle(all_available_topics.begin(),
                        all_available_topics.end());
    for (auto& topic_ref : all_available_topics) {
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
      const std::optional<gfx::Size>& asset_size) {
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
  struct TopicSet {
    // Not mutated after DynamicImageProvider's constructor.
    TopicReferenceVector topics;
    // Incremented each time a topic is picked from the set and loops back to
    // 0 when all topics from all TopicSets have been exhausted.
    size_t current_topic_idx = 0;
  };
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

class AmbientAnimationPhotoProvider::StaticImageAssetImpl
    : public cc::SkottieFrameDataProvider::ImageAsset {
 public:
  StaticImageAssetImpl(std::string_view asset_id,
                       const AmbientAnimationStaticResources& static_resources)
      : image_(static_resources.GetStaticImageAsset(asset_id)) {
    DCHECK(!IsCustomizableLottieId(asset_id));
    DCHECK(!image_.isNull())
        << "Static image asset " << asset_id << " is unknown.";
    DVLOG(1) << "Loaded static asset " << asset_id;
  }

  cc::SkottieFrameData GetFrameData(float t, float scale_factor) override {
    if (!enabled_)
      return cc::SkottieFrameData();

    if (!current_frame_data_.image ||
        current_frame_data_scale_factor_ != scale_factor) {
      current_frame_data_ = BuildSkottieFrameData(image_, scale_factor);
      current_frame_data_scale_factor_ = scale_factor;
    }
    return current_frame_data_;
  }

  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  // Private destructor since cc::SkottieFrameDataProvider::ImageAsset is a
  // ref-counted API.
  ~StaticImageAssetImpl() override = default;

  const gfx::ImageSkia image_;
  cc::SkottieFrameData current_frame_data_;
  float current_frame_data_scale_factor_ = 0;
  bool enabled_ = true;
};

class AmbientAnimationPhotoProvider::DynamicImageAssetImpl
    : public cc::SkottieFrameDataProvider::ImageAsset {
 public:
  DynamicImageAssetImpl(
      std::string_view asset_id,
      std::optional<gfx::Size> size,
      const base::WeakPtr<AmbientAnimationPhotoProvider>& provider)
      : asset_id_(asset_id), size_(std::move(size)), provider_(provider) {
    DCHECK(provider_);
    if (!ambient::util::ParseDynamicLottieAssetId(asset_id, parsed_asset_id_)) {
      LOG(DFATAL) << "Animation file is invalid. Failed to parse dynamic "
                     "image asset id "
                  << asset_id;
    }
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

  PhotoWithDetails ExtractAssignedTopic() {
    current_frame_data_ = cc::SkottieFrameData();
    current_frame_data_scale_factor_ = kImageScaleFactorInvalid;
    return std::move(current_topic_);
  }

  bool HasAssignedTopic() const { return !current_topic_.photo.isNull(); }

  const std::optional<gfx::Size>& size() const { return size_; }

  const std::string& asset_id() const { return asset_id_; }
  const ambient::util::ParsedDynamicAssetId& parsed_asset_id() const {
    return parsed_asset_id_;
  }
  const std::string& position_id() const {
    return parsed_asset_id_.position_id;
  }
  int idx() const { return parsed_asset_id_.idx; }

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
  ambient::util::ParsedDynamicAssetId parsed_asset_id_;
  const std::optional<gfx::Size> size_;
  const base::WeakPtr<AmbientAnimationPhotoProvider> provider_;
  // Last animation frame timestamp that was observed.
  float last_observed_animation_timestamp_ = kAnimationTimestampInvalid;
  cc::SkottieFrameData current_frame_data_;
  float current_frame_data_scale_factor_ = kImageScaleFactorInvalid;
  // The original topic off of which |current_frame_data_| was built. May have
  // multiple scale representations in its image in the event that a different
  // scale factor is required while rendering.
  PhotoWithDetails current_topic_;
};

bool AmbientAnimationPhotoProvider::OrderDynamicAssetsByIdx::operator()(
    const scoped_refptr<DynamicImageAssetImpl>& asset_l,
    const scoped_refptr<DynamicImageAssetImpl>& asset_r) const {
  DCHECK(asset_l);
  DCHECK(asset_r);
  return asset_l->idx() < asset_r->idx();
}

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
    std::string_view asset_id,
    const base::FilePath& resource_path,
    const std::optional<gfx::Size>& size) {
  // Note in practice, all of the image assets are loaded one time by Skottie
  // when the animation is initially loaded. So the set of assets does not
  // change once the animation starts rendering.
  if (IsCustomizableLottieId(asset_id)) {
    auto dynamic_asset = base::MakeRefCounted<DynamicImageAssetImpl>(
        asset_id, size, weak_factory_.GetWeakPtr());
    dynamic_assets_per_position_[dynamic_asset->position_id()].insert(
        dynamic_asset);
    ++total_num_dynamic_assets_;
    return dynamic_asset;
  } else {
    // For static assets, the |size| isn't needed. It should match the size of
    // the image loaded from animation's |static_resources_| since that is the
    // very image created by UX when the animation was built.
    auto static_asset = base::MakeRefCounted<StaticImageAssetImpl>(
        asset_id, *static_resources_);
    const auto hash_id = cc::HashSkottieResourceId(asset_id);
    static_assets_[hash_id] = static_asset;
    if (hash_id ==
        cc::HashSkottieResourceId(ambient::resources::kTreeShadowAssetId)) {
      static_asset->set_enabled(enable_tree_shadow_);
    }
    return static_asset;
  }
}

void AmbientAnimationPhotoProvider::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void AmbientAnimationPhotoProvider::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

bool AmbientAnimationPhotoProvider::ToggleStaticImageAsset(
    cc::SkottieResourceIdHash asset_id,
    bool enabled) {
  auto iter = static_assets_.find(asset_id);
  if (iter == static_assets_.end()) {
    // When the view is first created, all assets might not be loaded yet. Store
    // the `enabled` state to apply on the tree shadow asset when it is loaded.
    enable_tree_shadow_ = enabled;
  } else {
    iter->second->set_enabled(enabled);
  }
  return true;
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
  RotateDynamicAssetTopics();
  DynamicImageProvider image_provider(GetTopicsToChooseFrom());
  for (const auto& [_, dynamic_asset_set] : dynamic_assets_per_position_) {
    for (const auto& dynamic_asset : dynamic_asset_set) {
      bool asset_already_has_assigned_topic =
          pending_dynamic_asset_topics_.contains(dynamic_asset.get());
      if (asset_already_has_assigned_topic)
        continue;

      pending_dynamic_asset_topics_.emplace(
          dynamic_asset.get(),
          image_provider.GetTopicForAssetSize(dynamic_asset->size()));
    }
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

// For each position, the topic assigned to asset with index <i + 1> gets
// assigned to the asset with index <i>. Ultimately, the asset with the highest
// index at each position is left without an assigned topic. See comments at
// the top of the file for an example.
void AmbientAnimationPhotoProvider::RotateDynamicAssetTopics() {
  for (const auto& [_, dynamic_asset_set] : dynamic_assets_per_position_) {
    DCHECK(!dynamic_asset_set.empty());
    auto current_asset = dynamic_asset_set.begin();
    auto next_asset = std::next(current_asset);
    for (; next_asset != dynamic_asset_set.end();
         ++current_asset, ++next_asset) {
      // HasAssignedTopic() should only be false on the very first frame.
      if ((*next_asset)->HasAssignedTopic()) {
        pending_dynamic_asset_topics_[current_asset->get()] =
            (*next_asset)->ExtractAssignedTopic();
      }
    }
  }
}

std::vector<std::reference_wrapper<const PhotoWithDetails>>
AmbientAnimationPhotoProvider::GetTopicsToChooseFrom() const {
  const base::circular_deque<PhotoWithDetails>& all_available_topics =
      backend_model_->all_decoded_topics();
  size_t num_assets_without_assigned_topic =
      total_num_dynamic_assets_ - pending_dynamic_asset_topics_.size();
  // Clamp |num_assets_without_assigned_topic| in case the controller is having
  // a hard time preparing new topics (ex: network congestion) and there are
  // minimal topics in the model (1 is the bare minimum).
  size_t num_available_topics = all_available_topics.size();
  size_t num_topics_to_choose_from =
      std::min(num_assets_without_assigned_topic, num_available_topics);
  // |all_available_topics| is ordered from least recent to most recent, so
  // choose from the topics beginning at the end of the queue.
  std::vector<std::reference_wrapper<const PhotoWithDetails>>
      topics_to_choose_from;
  auto range_begin = all_available_topics.rbegin();
  auto range_end = range_begin + num_topics_to_choose_from;
  for (auto topic_iter = range_begin; topic_iter != range_end; ++topic_iter) {
    topics_to_choose_from.push_back(std::cref(*topic_iter));
  }
  return topics_to_choose_from;
}

void AmbientAnimationPhotoProvider::NotifyObserverOfNewTopics() {
  base::flat_map<ambient::util::ParsedDynamicAssetId,
                 std::reference_wrapper<const PhotoWithDetails>>
      new_topics;
  for (const auto& [asset, topic] : pending_dynamic_asset_topics_) {
    new_topics.emplace(asset->parsed_asset_id(), std::cref(topic));
  }
  for (Observer& obs : observers_) {
    obs.OnDynamicImageAssetsRefreshed(new_topics);
  }
}

}  // namespace ash
