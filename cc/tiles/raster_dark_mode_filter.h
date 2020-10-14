// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_RASTER_DARK_MODE_FILTER_H_
#define CC_TILES_RASTER_DARK_MODE_FILTER_H_

#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace cc {

// This class provides an interface/wrapeer over blink::DarkModeFilter. The APIs
// in this interface are thread-safe and can be used concurrently from any
// thread. The interface would be created and destroyed on main thread. Derived
// classes should ensure making these APIs thread-safe.
class CC_EXPORT RasterDarkModeFilter {
 public:
  // Update this enum based on enum DarkModeResult declared in
  // third_party/blink/renderer/platform/graphics/dark_mode_types.h
  enum class Result : uint8_t {
    kDoNotApplyFilter,
    kApplyFilter,
    kNotClassified
  };

  virtual ~RasterDarkModeFilter() = default;

  // For result kApplyFilter, call GetImageFilter() and for kNotClassified call
  // ApplyToImage() to get the dark mode image filter.
  virtual Result AnalyzeShouldApplyToImage(const SkIRect& src,
                                           const SkIRect& dst) const = 0;
  // Ensure pixmap has decoded data before calling this API.
  virtual sk_sp<SkColorFilter> ApplyToImage(const SkPixmap& pixmap,
                                            const SkIRect& src,
                                            const SkIRect& dst) const = 0;
  // Can be called only if AnalyzeShouldApplyToImage() returns kApplyFilter
  // result. This does not require decoding the image.
  virtual sk_sp<SkColorFilter> GetImageFilter() const = 0;
};

}  // namespace cc

#endif  // CC_TILES_RASTER_DARK_MODE_FILTER_H_
