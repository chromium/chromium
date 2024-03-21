// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_PROVIDER_H_
#define ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_PROVIDER_H_

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "cc/paint/skottie_frame_data_provider.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace ash {

class AmbientAnimationStaticResources;

// The |SkottieFrameDataProvider| implementation for ambient mode animations is
// tied to a single animation instance for its lifetime. It has 2 purposes:
// 1) Load "static" image assets (those that are fixed for the lifetime of the
//    animation) from storage. These are fixtures in the animation design such
//    as background images and are only loaded once in the animation's lifetime.
// 2) Pull images from the |AmbientBackendModel| for "dynamic" image assets,
//    which are the spots where the photos of interest go. Currently, a new set
//    of images are loaded from the model at the start of each new animation
//    cycle.
class ASH_EXPORT AmbientAnimationPhotoProvider
    : public cc::SkottieFrameDataProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked whenever the dynamic assets in the animation are assigned a new
    // set of photos (reflected in |new_topics|). There shall be one entry in
    // |new_topics| for each dynamic asset in the animation, where the entry's
    // value is the topic that the asset was just assigned.
    //
    // Note in the event that the same topic is assigned to multiple dynamic
    // assets, that topic will appear multiple times in |new_topics|.
    virtual void OnDynamicImageAssetsRefreshed(
        const base::flat_map<ambient::util::ParsedDynamicAssetId,
                             std::reference_wrapper<const PhotoWithDetails>>&
            new_topics) = 0;

   protected:
    ~Observer() override = default;
  };

  AmbientAnimationPhotoProvider(
      const AmbientAnimationStaticResources* static_resources,
      const AmbientBackendModel* backend_model);
  ~AmbientAnimationPhotoProvider() override;

  scoped_refptr<ImageAsset> LoadImageAsset(
      std::string_view resource_id,
      const base::FilePath& resource_path,
      const std::optional<gfx::Size>& size) override;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Sets whether the static image asset in the animation with the given
  // |asset_id| is enabled or not. If an image asset is disabled, the rest of
  // the animation can still render properly; the specified static image asset
  // will just be missing. By default, all static image assets are enabled
  // until specified otherwise by the caller.
  //
  // Returns true on success; false if |asset_id| is unknown.
  bool ToggleStaticImageAsset(cc::SkottieResourceIdHash asset_id, bool enabled);

 private:
  class DynamicImageAssetImpl;
  class StaticImageAssetImpl;

  struct OrderDynamicAssetsByIdx {
    bool operator()(const scoped_refptr<DynamicImageAssetImpl>& asset_l,
                    const scoped_refptr<DynamicImageAssetImpl>& asset_r) const;
  };

  using DynamicAssetSet = base::flat_set<scoped_refptr<DynamicImageAssetImpl>,
                                         OrderDynamicAssetsByIdx>;

  PhotoWithDetails GenerateNextTopicForDynamicAsset(
      const DynamicImageAssetImpl& asset);
  PhotoWithDetails ExtractPendingTopicForDynamicAsset(
      const DynamicImageAssetImpl& asset);
  void RotateDynamicAssetTopics();

  std::vector<std::reference_wrapper<const PhotoWithDetails>>
  GetTopicsToChooseFrom() const;

  void NotifyObserverOfNewTopics();
  void RecordDynamicAssetMetrics();

  // Whether the tree shadow asset should be set. See the comment in
  // `AmbientAnimationView::OnViewBoundsChanged()`.
  bool enable_tree_shadow_ = false;

  // Unowned pointers. Must outlive the |AmbientAnimationPhotoProvider|.
  const raw_ptr<const AmbientAnimationStaticResources> static_resources_;
  const raw_ptr<const AmbientBackendModel> backend_model_;

  // Map's key is hash of the static image asset's string id.
  base::flat_map<cc::SkottieResourceIdHash, scoped_refptr<StaticImageAssetImpl>>
      static_assets_;
  base::flat_map</*position_id*/ std::string, DynamicAssetSet>
      dynamic_assets_per_position_;
  size_t total_num_dynamic_assets_ = 0;
  base::flat_map<const DynamicImageAssetImpl*, PhotoWithDetails>
      pending_dynamic_asset_topics_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<AmbientAnimationPhotoProvider> weak_factory_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_PROVIDER_H_
