// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_FRAME_DATA_PROVIDER_H_
#define CC_PAINT_SKOTTIE_FRAME_DATA_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_frame_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cc {

// A Chromium-specific version of the skresources::ResourceProvider API, which
// allows the code driving the animation to specify which image should be used
// for each asset in each frame of the animation. Callers rendering Skottie
// animations that may have images embedded in them must implement this API. In
// the most basic case where an image asset does not change throughout the
// course of the animation, the same image can be provided for every frame. But
// more complex logic such as providing a different image at the start of each
// animation cycle can be implemented if desired.
//
// Implementations are not required to be thread-safe (the provider and its
// ImageAssets shall always be invoked from the same sequence).
class CC_PAINT_EXPORT SkottieFrameDataProvider {
 public:
  class CC_PAINT_EXPORT ImageAsset
      : public base::RefCountedThreadSafe<ImageAsset> {
   public:
    // Returns the image to use for an asset in a frame of a skottie animation.
    // If absl::nullopt is returned, the most recently provided image for this
    // asset is reused when the frame is rendered. Thus, the ImageAsset may
    // return "null" if: a) The most recent image intentionally should be
    // reused or b) The provider knows that this particular asset does not
    // appear at the specified timestamp of the animation.
    //
    // |t|: See skresources::ImageAsset::getFrame(). Same semantics. Specifies
    //      the frame of interest in the animation that's about to be rendered.
    // |scale_factor|: See |image_scale| in gfx::Canvas. Can be used to generate
    //                 a PaintImage from a gfx::ImageSkia instance.
    virtual absl::optional<SkottieFrameData> GetFrameData(
        float t,
        float scale_factor) = 0;

   protected:
    virtual ~ImageAsset() = default;

   private:
    friend class base::RefCountedThreadSafe<ImageAsset>;
  };

  virtual ~SkottieFrameDataProvider() = default;

  // Loads the image asset in the animation with the given |resource_id|, as it
  // appears in the lottie json file. The ImageAsset instance that's returned
  // for the given |resource_id| gets re-used for the lifetime of the animation;
  // LoadImageAsset() is not called multiple times for the same |resource_id|.
  // The returned value must never be null.
  virtual scoped_refptr<ImageAsset> LoadImageAsset(
      base::StringPiece resource_id,
      const base::FilePath& resource_path) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_FRAME_DATA_PROVIDER_H_
