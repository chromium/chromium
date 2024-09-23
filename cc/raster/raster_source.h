// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_RASTER_SOURCE_H_
#define CC_RASTER_RASTER_SOURCE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/image_id.h"
#include "cc/paint/scroll_offset_map.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/color_space.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace gfx {
class AxisTransform2d;
}  // namespace gfx

namespace cc {
class DisplayItemList;
class ImageProvider;

class CC_EXPORT RasterSource : public base::RefCountedThreadSafe<RasterSource> {
 public:
  struct CC_EXPORT PlaybackSettings {
    PlaybackSettings();
    PlaybackSettings(const PlaybackSettings&);
    PlaybackSettings(PlaybackSettings&&);
    ~PlaybackSettings();

    // If set to true, we should use LCD text.
    bool use_lcd_text = true;

    // Specifies the sample count if MSAA is enabled for this tile.
    int msaa_sample_count = 0;

    // Visible hint, GPU may use it as a hint to schedule raster tasks.
    bool visible = false;

    // The HDR headroom to use when tone mapping content.
    float hdr_headroom = 1.f;

    raw_ptr<ImageProvider> image_provider = nullptr;
    raw_ptr<const ScrollOffsetMap> raster_inducing_scroll_offsets = nullptr;
  };

  RasterSource(const RasterSource&) = delete;
  RasterSource& operator=(const RasterSource&) = delete;

  // This is useful for rastering into tiles. |canvas| is expected to be backed
  // by a tile, with a default state. |raster_transform| will be applied to the
  // display list, rastering the list into the "content space".
  // |canvas_bitmap_rect| defines the extent of the tile in the content space,
  // i.e. contents in the rect will be cropped and translated onto the canvas.
  // |canvas_playback_rect| can be used to replay only part of the recording in,
  // the content space, so only a sub-rect of the tile gets rastered.
  //
  // Note that this should only be called after the image decode controller has
  // been set, which happens during commit.
  void PlaybackToCanvas(SkCanvas* canvas,
                        const gfx::Size& content_size,
                        const gfx::Rect& canvas_bitmap_rect,
                        const gfx::Rect& canvas_playback_rect,
                        const gfx::AxisTransform2d& raster_transform,
                        const PlaybackSettings& settings) const;

  // Returns whether the given rect at given scale is of solid color in
  // this raster source, as well as the solid color value.
  //
  // If max_ops_to_analyze is set, changes the default maximum number of
  // operations to analyze before giving up. Careful: even very simple lists can
  // have more than one operation, so 1 may not be the value you're looking
  // for. For instance, solid color tiles generated for views have 3
  // operations. See comments in TileManager::AssignGpuMemoryToTils() for
  // details.
  bool PerformSolidColorAnalysis(gfx::Rect content_rect,
                                 SkColor4f* color,
                                 int max_ops_to_analyze = 1) const;

  // Returns true iff the whole raster source is of solid color.
  bool IsSolidColor() const;

  // Returns the color of the raster source if it is solid color. The results
  // are unspecified if IsSolidColor returns false.
  SkColor4f GetSolidColor() const;

  // Returns the recorded layer size of this raster source.
  gfx::Size size() const { return size_; }

  // Returns the content size of this raster source at a particular scale.
  gfx::Size GetContentSize(const gfx::Vector2dF& content_scale) const;

  // Return true iff this raster source can raster the given rect in layer
  // space.
  bool IntersectsRect(const gfx::Rect& layer_rect) const;

  // Returns true if this raster source has anything to rasterize.
  bool HasRecordings() const;

  // Valid rectangle in which anything is recorded and can be rastered from.
  gfx::Rect recorded_bounds() const {
    // TODO(crbug.com/41490692): Create tiling for directly composited images
    // based on the recorded bounds.
    return directly_composited_image_info_ ? gfx::Rect(size_)
                                           : recorded_bounds_;
  }

  // Tracing functionality.
  void DidBeginTracing();
  void AsValueInto(base::trace_event::TracedValue* array) const;

  const scoped_refptr<const DisplayItemList>& GetDisplayItemList() const {
    return display_list_;
  }

  float recording_scale_factor() const { return recording_scale_factor_; }

  SkColor4f background_color() const { return background_color_; }

  bool requires_clear() const { return requires_clear_; }

  size_t* max_op_size_hint() { return &max_op_size_hint_; }

  const std::optional<DirectlyCompositedImageInfo>&
  directly_composited_image_info() const {
    return directly_composited_image_info_;
  }

  void set_debug_name(const std::string& name) { debug_name_ = name; }
  const std::string& debug_name() const { return debug_name_; }

 protected:
  // RecordingSource is the only class that can create a raster source.
  friend class RecordingSource;
  friend class base::RefCountedThreadSafe<RasterSource>;

  explicit RasterSource(const RecordingSource& other);
  virtual ~RasterSource();

  void ClearForOpaqueRaster(SkCanvas* raster_canvas,
                            const gfx::AxisTransform2d& raster_transform,
                            const gfx::Size& content_size,
                            const gfx::Rect& canvas_bitmap_rect,
                            const gfx::Rect& canvas_playback_rect) const;

  // Raster the display list of this raster source into the given canvas.
  // Canvas states such as CTM and clip region will be respected.
  // This function will replace pixels in the clip region without blending.
  //
  // Virtual for testing.
  virtual void PlaybackDisplayListToCanvas(
      SkCanvas* canvas,
      const PlaybackSettings& settings) const;

  // The serialized size for the largest op in this RasterSource. This is
  // accessed only on the raster threads with the context lock acquired.
  size_t max_op_size_hint_ =
      gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;

  // These members are const as this raster source may be in use on another
  // thread and so should not be touched after construction.
  const scoped_refptr<const DisplayItemList> display_list_;
  const SkColor4f background_color_;
  const bool requires_clear_;
  const bool is_solid_color_;
  const SkColor4f solid_color_;
  const gfx::Rect recorded_bounds_;
  const gfx::Size size_;
  const int slow_down_raster_scale_factor_for_debug_;
  const float recording_scale_factor_;
  std::optional<DirectlyCompositedImageInfo> directly_composited_image_info_;
  // Used for debugging and tracing.
  std::string debug_name_;
};

}  // namespace cc

#endif  // CC_RASTER_RASTER_SOURCE_H_
