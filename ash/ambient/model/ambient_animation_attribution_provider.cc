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
//    * "_CrOS_Photo_PositionA_1"
//    * "_CrOS_Photo_PositionB_1"
//    ...
//    * "_CrOS_Photo_Position<P>_N"
//    Attribution text node names:
//    * "_CrOS_AttributionText1"
//    * "_CrOS_AttributionText2"
//    ...
//    * "_CrOS_AttributionTextN"
//
//    To assign an image asset to an attribution text node, sort the
//    asset ids by index first and position second (this is taken care of
//    already by ParsedDynamicAssetId's comparison operator and the use of
//    a sorted base::flat_map for |new_topics| below):
//    1) "_CrOS_Photo_PositionA_1" (Index 1 Position A)
//    2) "_CrOS_Photo_PositionB_1" (Index 1 Position B)
//    3) "_CrOS_Photo_PositionA_2" (Index 2 Position A)
//    4) "_CrOS_Photo_PositionB_2" (Index 2 Position B)
//
//    And sort the attribution text nodes by their name:
//    1) "_CrOS_AttributionText1"
//    2) "_CrOS_AttributionText2"
//    3) "_CrOS_AttributionText3"
//    4) "_CrOS_AttributionText4"
//
//    Afterwards, assign sorted asset <i> to sorted attribution node <i>. Note
//    the animation is allowed to have fewer attribution nodes than dynamic
//    image assets. In this case, the dynamic image assets left without a
//    corresponding attribution text node are just ignored by design.
//
// 2) If a photo has no attribution (an empty string), just set its
//    corresponding text node to be blank (an empty string). This is a
//    corner case though. In practice, either all of the photos in the set
//    should have an associated attribution, or none do.
void AmbientAnimationAttributionProvider::OnDynamicImageAssetsRefreshed(
    const base::flat_map<ambient::util::ParsedDynamicAssetId,
                         std::reference_wrapper<const PhotoWithDetails>>&
        new_topics) {
  DCHECK_GE(new_topics.size(), attribution_node_ids_.size())
      << "All ambient-mode animations should have at least as many dynamic "
         "image assets as text attribution nodes.";
  auto new_topics_iter = new_topics.begin();
  auto attribution_node_ids_iter = attribution_node_ids_.begin();
  for (; attribution_node_ids_iter != attribution_node_ids_.end();
       ++new_topics_iter, ++attribution_node_ids_iter) {
    const std::string& attribution_text = new_topics_iter->second.get().details;
    cc::SkottieResourceIdHash attribution_node_id = *attribution_node_ids_iter;
    DCHECK(animation_->text_map().contains(attribution_node_id));
    animation_->text_map().at(attribution_node_id).SetText(attribution_text);
  }
}

}  // namespace ash
