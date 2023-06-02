// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_RASTER_DARK_MODE_FILTER_H_
#define CC_TILES_RASTER_DARK_MODE_FILTER_H_

#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace cc {

class ColorFilter;

// This class provides an interface/wrapper over blink::DarkModeFilter. The APIs
// in this interface are thread-safe and can be used concurrently from any
// thread. The interface would be created and destroyed on main thread. Derived
// classes should ensure making these APIs thread-safe.
class CC_EXPORT RasterDarkModeFilter {
 public:
  virtual ~RasterDarkModeFilter() = default;

  // Ensure pixmap has decoded data before calling this API.
  virtual sk_sp<ColorFilter> ApplyToImage(const SkPixmap& pixmap,
                                          const SkIRect& src) const = 0;
};

}  // namespace cc

#endif  // CC_TILES_RASTER_DARK_MODE_FILTER_H_
