// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_PROVIDER_H_
#define ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_PROVIDER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/paint/skottie_frame_data_provider.h"

namespace ash {

class AmbientAnimationStaticResources;
class AmbientBackendModel;

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
  AmbientAnimationPhotoProvider(
      const AmbientAnimationStaticResources* static_resources,
      const AmbientBackendModel* backend_model);
  ~AmbientAnimationPhotoProvider() override;

  scoped_refptr<ImageAsset> LoadImageAsset(
      base::StringPiece resource_id,
      const base::FilePath& resource_path) override;

 private:
  class DynamicImageAssetImpl;

  void RefreshDynamicImageAssets();

  // Unowned pointers. Must outlive the |AmbientAnimationPhotoProvider|.
  const AmbientAnimationStaticResources* const static_resources_;
  const AmbientBackendModel* const backend_model_;

  std::vector<scoped_refptr<DynamicImageAssetImpl>> dynamic_assets_;
  base::WeakPtrFactory<AmbientAnimationPhotoProvider> weak_factory_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_PROVIDER_H
