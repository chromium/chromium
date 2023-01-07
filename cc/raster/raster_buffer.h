// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_RASTER_BUFFER_H_
#define CC_RASTER_RASTER_BUFFER_H_

#include <stdint.h>

#include "cc/cc_export.h"
#include "cc/raster/raster_source.h"
#include "ui/gfx/geometry/rect.h"

class GURL;

namespace gfx {
class AxisTransform2d;
}  // namespace gfx

namespace cc {

class CC_EXPORT RasterBuffer {
 public:
  RasterBuffer();
  virtual ~RasterBuffer();

  virtual void Playback(const RasterSource* raster_source,
                        const gfx::Rect& raster_full_rect,
                        const gfx::Rect& raster_dirty_rect,
                        uint64_t new_content_id,
                        const gfx::AxisTransform2d& transform,
                        const RasterSource::PlaybackSettings& playback_settings,
                        const GURL& url) = 0;

  // Returns true if Playback() can be invoked at background thread priority. To
  // avoid priority inversions, this should return false if Playback() acquires
  // resources that are also acquired at normal thread priority.
  // https://crbug.com/1072756.
  virtual bool SupportsBackgroundThreadPriority() const = 0;
};

}  // namespace cc

#endif  // CC_RASTER_RASTER_BUFFER_H_
