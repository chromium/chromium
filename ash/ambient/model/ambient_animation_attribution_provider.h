// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_ATTRIBUTION_PROVIDER_H_
#define ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_ATTRIBUTION_PROVIDER_H_

#include <vector>

#include "ash/ambient/model/ambient_animation_photo_provider.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace lottie {
class Animation;
}  // namespace lottie

namespace ash {

// "Attribution" refers to the text credits that may optionally accompany each
// photo that's assigned to a dynamic asset in an animation. The Lottie files
// for ambient mode have a placeholder for each dynamic asset where its
// attribution text should go, and AmbientAnimationAttributionProvider's job is
// to fill in the placeholders with the appropriate text credits.
class ASH_EXPORT AmbientAnimationAttributionProvider
    : public AmbientAnimationPhotoProvider::Observer {
 public:
  AmbientAnimationAttributionProvider(
      AmbientAnimationPhotoProvider* photo_provider,
      lottie::Animation* animation);
  AmbientAnimationAttributionProvider(
      const AmbientAnimationAttributionProvider&) = delete;
  AmbientAnimationAttributionProvider& operator=(
      const AmbientAnimationAttributionProvider&) = delete;
  ~AmbientAnimationAttributionProvider() override;

  // AmbientAnimationPhotoProvider::Observer implementation:
  void OnDynamicImageAssetsRefreshed(
      const base::flat_map<ambient::util::ParsedDynamicAssetId,
                           std::reference_wrapper<const PhotoWithDetails>>&
          new_topics) override;

 private:
  const raw_ptr<lottie::Animation> animation_;
  // Set of text nodes in the animation that should hold attribution for a
  // photo. It is expected that the size of this vector matches the number of
  // dynamic image assets in the animation (1 for each photo).
  const std::vector<cc::SkottieResourceIdHash> attribution_node_ids_;
  base::ScopedObservation<AmbientAnimationPhotoProvider,
                          AmbientAnimationPhotoProvider::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_ATTRIBUTION_PROVIDER_H_
