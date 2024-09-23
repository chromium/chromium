// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_MRU_RESOURCE_PROVIDER_H_
#define CC_PAINT_SKOTTIE_MRU_RESOURCE_PROVIDER_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/modules/skresources/include/SkResources.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// Provides Skottie the most recent SkImage that was returned by a
// SkottieWrapper::FrameDataCallback for each ImageAsset. Note this is a
// "multi-frame" ResourceProvider, so the caller is capable of supporting
// animations where the image assets do/don't change between frames.
//
// Not thread-safe. All public methods must be called from the sequence that
// SkottieMRUResourceProvider is constructed on.
class CC_PAINT_EXPORT SkottieMRUResourceProvider
    : public skresources::ResourceProvider {
 public:
  using FrameDataCallback = SkottieWrapper::FrameDataCallback;

  SkottieMRUResourceProvider(FrameDataCallback frame_data_cb,
                             std::string_view animation_json);
  SkottieMRUResourceProvider(const SkottieMRUResourceProvider&) = delete;
  SkottieMRUResourceProvider& operator=(const SkottieMRUResourceProvider&) =
      delete;
  ~SkottieMRUResourceProvider() override;

  // Contains the metadata for all currently known external ImageAssets in the
  // animation.
  const SkottieResourceMetadataMap& GetImageAssetMetadata() const;

 private:
  // skresources::ResourceProvider implementation:
  sk_sp<skresources::ImageAsset> loadImageAsset(
      const char resource_path[],
      const char resource_name[],
      const char resource_id[]) const override;

  const SkottieWrapper::FrameDataCallback frame_data_cb_;
  const base::flat_map</*asset_id*/ std::string, gfx::Size> image_asset_sizes_;
  // SkResources.h declares loadImageAsset() as a "const" method. Although the
  // method is logically const, these book-keeping members need to be updated in
  // that method. Hence, they're marked "mutable".
  mutable SkottieResourceMetadataMap image_asset_metadata_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_MRU_RESOURCE_PROVIDER_H_
