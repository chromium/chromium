// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_FRAME_DATA_H_
#define CC_PAINT_SKOTTIE_FRAME_DATA_H_

#include "base/containers/flat_map.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace cc {

// The equivalent of skresources::ImageAsset::FrameData, except expressed in
// terms of Chromium Compositor constructs rather than Skia constructs.
// Represents the image to use for an asset in one frame of a Skottie animation.
//
// There's currently no use case for |skresources::ImageAsset::FrameData.matrix|
// so it is omitted for now.
struct CC_PAINT_EXPORT SkottieFrameData {
  // PaintImage is preferable at the compositor layer instead of a "raw"
  // SkImage. It not only is more well supported for circulating through the
  // compositor/graphics pipeline, but also gives the client the most
  // versatility for how the image is "backed" (ex: a PaintImageGenerator or
  // PaintRecord can be used).
  PaintImage image;
  // Chromium version of SkSamplingOptions. Controls resampling quality if the
  // image needs to be resized when rendering.
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kLow;
};

// Map from asset id to the image to use for that asset.
using SkottieFrameDataMap =
    base::flat_map<SkottieResourceIdHash, SkottieFrameData>;

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_FRAME_DATA_H_
