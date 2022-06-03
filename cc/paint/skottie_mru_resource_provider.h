// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_MRU_RESOURCE_PROVIDER_H_
#define CC_PAINT_SKOTTIE_MRU_RESOURCE_PROVIDER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/modules/skresources/include/SkResources.h"

namespace cc {

// Provides Skottie the most recent SkImage that was "set" by the caller for
// each ImageAsset. Note this is a "multi-frame" ResourceProvider, so the caller
// is capable of supporting animations where the image assets do/don't change
// between frames.
//
// Not thread-safe. All methods must be called from the same sequence.
class CC_PAINT_EXPORT SkottieMRUResourceProvider
    : public skresources::ResourceProvider {
 public:
  using FrameData = skresources::ImageAsset::FrameData;

  // Not thread-safe. All methods must be called from the same sequence; this
  // should be the sequence on which skottie::Animation::render() is called.
  // Note however that this may be a different sequence than the one that the
  // ImageAsset was originally constructed/loaded on.
  class CC_PAINT_EXPORT ImageAsset : public skresources::ImageAsset {
   public:
    ImageAsset(const ImageAsset&) = delete;
    ImageAsset& operator=(const ImageAsset&) = delete;

    ~ImageAsset() override;

    // Sets the |frame_data| to be used for this asset in all future animation
    // frames until the next SetCurrentFrameData() call overwrites it.
    //
    // There are no requirements for |frame_data.image|'s backing; it is up to
    // the caller to decide.
    //
    // It is an error if no FrameData has ever been set for an asset and Skottie
    // requests FrameData for it (i.e. the caller should ensure all ImageAssets
    // are "set" with the appropriate FrameData before requesting Skottie render
    // the next frame of interest).
    void SetCurrentFrameData(FrameData frame_data);

   private:
    friend class SkottieMRUResourceProvider;

    // skresources::ImageAsset implementation:
    bool isMultiFrame() override;
    FrameData getFrameData(float t) override;

    explicit ImageAsset(base::StringPiece resource_id);

    const std::string resource_id_;
    FrameData current_frame_data_ GUARDED_BY_CONTEXT(render_sequence_checker_);
    SEQUENCE_CHECKER(render_sequence_checker_);
  };

  using ImageAssetMap =
      base::flat_map<SkottieResourceIdHash, sk_sp<ImageAsset>>;

  SkottieMRUResourceProvider();
  SkottieMRUResourceProvider(const SkottieMRUResourceProvider&) = delete;
  SkottieMRUResourceProvider& operator=(const SkottieMRUResourceProvider&) =
      delete;
  ~SkottieMRUResourceProvider() override;

  // Contains the metadata for all currently known ImageAssets in the animation.
  const SkottieResourceMetadataMap& GetImageAssetMetadata() const;

  // Contains all ImageAssets in the animation that Skottie has loaded thus far.
  const ImageAssetMap& GetImageAssetMap() const;

 private:
  // skresources::ResourceProvider implementation:
  sk_sp<skresources::ImageAsset> loadImageAsset(
      const char resource_path[],
      const char resource_name[],
      const char resource_id[]) const override;

  // SkResources.h declares loadImageAsset() as a "const" method. Although the
  // method is logically const, these book-keeping members need to be updated in
  // that method. Hence, they're marked "mutable".
  mutable SkottieResourceMetadataMap image_asset_metadata_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mutable ImageAssetMap image_asset_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_MRU_RESOURCE_PROVIDER_H_
