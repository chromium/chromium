// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_attribution_provider.h"

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/lottie/animation.h"

namespace ash {

namespace {

// Not all text nodes in the animation are necessarily ones that should hold
// photo attribution. Some animations may have static text embedded in them.
// Filter these out.
//
// The returned vector is intentionally sorted by the corresponding node name
// in string format:
// {
//   hash("CrOS_AttributionNode1"),
//   hash("CrOS_AttributionNode2"),
//   ...,
//   hash("CrOS_AttributionNodeN")
// }
// See "UX Guidance" below for why.
std::vector<cc::SkottieResourceIdHash> GetAttributionNodeIds(
    const cc::SkottieWrapper& skottie) {
  base::flat_set<std::string> attribution_node_names_sorted;
  for (const std::string& text_node_name : skottie.GetTextNodeNames()) {
    if (IsCustomizableLottieId(text_node_name))
      attribution_node_names_sorted.insert(text_node_name);
  }
  std::vector<cc::SkottieResourceIdHash> attribution_node_ids;
  for (const std::string& attribution_node_name :
       attribution_node_names_sorted) {
    attribution_node_ids.push_back(
        cc::HashSkottieResourceId(attribution_node_name));
  }
  return attribution_node_ids;
}

}  // namespace

AmbientAnimationAttributionProvider::AmbientAnimationAttributionProvider(
    AmbientAnimationPhotoProvider* photo_provider,
    lottie::Animation* animation)
    : animation_(animation),
      attribution_node_ids_(GetAttributionNodeIds(*animation_->skottie())) {
  DCHECK(animation_);
  observation_.Observe(photo_provider);
}

AmbientAnimationAttributionProvider::~AmbientAnimationAttributionProvider() =
    default;

// UX Guidance:
// 1) The dynamic image assets and attribution text nodes in an animation are
//    identified by 2 different sets of strings. Ex:
//    Dynamic image asset ids:
//    * "CrOS_AssetId1"
//    * "CrOS_AssetId2"
//    ...
//    * "CrOS_AssetIdN"
//    Attribution text node names:
//    * "CrOS_AttributionNode1"
//    * "CrOS_AttributionNode2"
//    ...
//    * "CrOS_AttributionNodeN"
//    Each attribution text node should be assigned the attribution of the
//    dynamic image asset who shares the same "index". "CrOS_AttributionNode1"
//    is assigned the attribution for "CrOS_AssetId1", "CrOS_AttributionNode2"
//    is assigned the attribution for "CrOS_AssetId2", and so on. This is easily
//    accomplished by sorting the asset ids and attribution nodes as strings
//    first, then iterating through them simultaneously when assigning. That is
//    why |new_topics| is a flat_map (whose keys are inherently sorted), and
//    GetAttributionNodeIds() returns the attribution node ids sorted by their
//    string names.
//
// 2) If a photo has no attribution (an empty string), just set its
//    corresponding text node to be blank (an empty string). This is a
//    corner case though. In practice, either all of the photos in the set
//    should have an associated attribution, or none do.
void AmbientAnimationAttributionProvider::OnDynamicImageAssetsRefreshed(
    const base::flat_map</*asset_id*/ std::string,
                         std::reference_wrapper<const PhotoWithDetails>>&
        new_topics) {
  DCHECK_EQ(new_topics.size(), attribution_node_ids_.size())
      << "All ambient-mode animations should have an equal number of text "
         "attribution nodes and dynamic image assets.";
  auto new_topics_iter = new_topics.begin();
  auto attribution_node_ids_iter = attribution_node_ids_.begin();
  for (; new_topics_iter != new_topics.end();
       ++new_topics_iter, ++attribution_node_ids_iter) {
    const std::string& attribution_text = new_topics_iter->second.get().details;
    cc::SkottieResourceIdHash attribution_node_id = *attribution_node_ids_iter;
    DCHECK(animation_->text_map().contains(attribution_node_id));
    animation_->text_map().at(attribution_node_id).SetText(attribution_text);
  }
}

}  // namespace ash
