// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_RECORDING_SOURCE_H_
#define CC_LAYERS_RECORDING_SOURCE_H_

#include <stddef.h>

#include "base/memory/ref_counted.h"
#include "cc/base/invalidation_region.h"
#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class ContentLayerClient;
class DisplayItemList;
class RasterSource;
class Region;

class CC_EXPORT RecordingSource {
 public:
  RecordingSource();
  RecordingSource(const RecordingSource&) = delete;
  ~RecordingSource();

  RecordingSource& operator=(const RecordingSource&) = delete;

  bool Update(const gfx::Size& layer_size,
              float recording_scale_factor,
              ContentLayerClient& content_layer_client,
              Region& invalidation);
  gfx::Size GetSize() const;
  const DisplayItemList* display_list() const { return display_list_.get(); }
  void SetEmptyBounds();
  void SetSlowdownRasterScaleFactor(int factor);
  void SetBackgroundColor(SkColor4f background_color);
  void SetRequiresClear(bool requires_clear);

  void SetNeedsDisplayRect(const gfx::Rect& layer_rect);

  scoped_refptr<RasterSource> CreateRasterSource() const;

  bool is_solid_color() const { return is_solid_color_; }

 protected:
  // TODO(crbug.com/1157714): For now this is different from gfx::Rect(size_)
  // in unit tests only. Remove this field and use display_list_->bounds().
  gfx::Rect recorded_viewport_;
  gfx::Size size_;
  int slow_down_raster_scale_factor_for_debug_ = 0;
  bool requires_clear_ = false;
  bool is_solid_color_ = false;
  SkColor4f solid_color_ = SkColors::kTransparent;
  SkColor4f background_color_ = SkColors::kTransparent;
  scoped_refptr<DisplayItemList> display_list_;
  float recording_scale_factor_ = 1.0f;

 private:
  void UpdateInvalidationForNewViewport(const gfx::Rect& old_recorded_viewport,
                                        const gfx::Rect& new_recorded_viewport,
                                        Region& invalidation);

  void UpdateDisplayItemList(scoped_refptr<DisplayItemList> display_list,
                             float recording_scale_factor,
                             Region& invalidation);
  void FinishDisplayItemListUpdate();

  friend class RasterSource;

  void DetermineIfSolidColor();

  InvalidationRegion invalidation_;
};

}  // namespace cc

#endif  // CC_LAYERS_RECORDING_SOURCE_H_
