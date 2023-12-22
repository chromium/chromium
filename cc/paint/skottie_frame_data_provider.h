// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_FRAME_DATA_PROVIDER_H_
#define CC_PAINT_SKOTTIE_FRAME_DATA_PROVIDER_H_

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skottie_frame_data.h"
#include "ui/gfx/geometry/size.h"

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
  class CC_PAINT_EXPORT ImageAsset : public base::RefCounted<ImageAsset> {
   public:
    // Returns the image to use for an asset in a frame of a skottie animation.
    // May return a blank SkottieFrameData instance with an empty |image|.
    // Skottie handles this gracefully and simply skips the image asset while
    // still rendering the rest of the frame.
    //
    // |t|: See skresources::ImageAsset::getFrame(). Same semantics. Specifies
    //      the frame of interest in the animation that's about to be rendered.
    // |scale_factor|: See |image_scale| in gfx::Canvas. Can be used to generate
    //                 a PaintImage from a gfx::ImageSkia instance.
    virtual SkottieFrameData GetFrameData(float t, float scale_factor) = 0;

   protected:
    virtual ~ImageAsset() = default;

   private:
    friend class base::RefCounted<ImageAsset>;
  };

  virtual ~SkottieFrameDataProvider() = default;

  // Loads the image asset in the animation with the given |resource_id|, as it
  // appears in the lottie json file. The ImageAsset instance that's returned
  // for the given |resource_id| gets re-used for the lifetime of the animation;
  // LoadImageAsset() is not called multiple times for the same |resource_id|.
  // The returned value must never be null.
  //
  // |size| contains this asset's dimensions as specified in the Lottie
  // animation file. Note that the ultimate image(s) returned by GetFrameData()
  // are not required to have dimensions that match this |size|. It's provided
  // here as a guide that implementations can optionally use to transform
  // their images if desired. May be null if the asset didn't have dimensions
  // specified in the Lottie file.
  virtual scoped_refptr<ImageAsset> LoadImageAsset(
      std::string_view resource_id,
      const base::FilePath& resource_path,
      const std::optional<gfx::Size>& size) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_FRAME_DATA_PROVIDER_H_
