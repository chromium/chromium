// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>

#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "cc/raster/raster_source.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/geometry/axis_transform2d.h"

namespace cc {

RasterBufferProvider::RasterBufferProvider() = default;

RasterBufferProvider::~RasterBufferProvider() = default;

namespace {

bool IsSupportedPlaybackToMemoryFormat(viz::SharedImageFormat format) {
  return (format == viz::SinglePlaneFormat::kRGBA_4444) ||
         (format == viz::SinglePlaneFormat::kRGBA_8888) ||
         (format == viz::SinglePlaneFormat::kBGRA_8888) ||
         (format == viz::SinglePlaneFormat::kRGBA_F16);
}

}  // anonymous namespace

// static
void RasterBufferProvider::PlaybackToMemory(
    void* memory,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    size_t stride,
    const RasterSource* raster_source,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    const gfx::AxisTransform2d& transform,
    const gfx::ColorSpace& target_color_space,
    bool gpu_compositing,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "RasterBufferProvider::PlaybackToMemory");

  DCHECK(IsSupportedPlaybackToMemoryFormat(format)) << format.ToString();

  SkColorType color_type = ToClosestSkColorType(gpu_compositing, format);

  // Uses kPremul_SkAlphaType since the result is not known to be opaque.
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kPremul_SkAlphaType,
                                       target_color_space.ToSkColorSpace());

  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  if (playback_settings.use_lcd_text) {
    surface_props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  }

  if (!stride)
    stride = info.minRowBytes();
  DCHECK_GT(stride, 0u);

  gfx::Size content_size = raster_source->GetContentSize(transform.scale());

  if ((format == viz::SinglePlaneFormat::kRGBA_8888) ||
      (format == viz::SinglePlaneFormat::kBGRA_8888) ||
      (format == viz::SinglePlaneFormat::kRGBA_F16)) {
    sk_sp<SkSurface> surface =
        SkSurfaces::WrapPixels(info, memory, stride, &surface_props);
    // There are some rare crashes where this doesn't succeed and may be
    // indicative of memory stomps elsewhere.  Instead of displaying
    // invalid content, just crash the renderer and try again.
    // See: http://crbug.com/721744.
    CHECK(surface);
    raster_source->PlaybackToCanvas(surface->getCanvas(), content_size,
                                    canvas_bitmap_rect, canvas_playback_rect,
                                    transform, playback_settings);
    return;
  }

  if (format == viz::SinglePlaneFormat::kRGBA_4444) {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info, &surface_props);
    // TODO(reveman): Improve partial raster support by reducing the size of
    // playback rect passed to PlaybackToCanvas. crbug.com/519070
    raster_source->PlaybackToCanvas(surface->getCanvas(), content_size,
                                    canvas_bitmap_rect, canvas_bitmap_rect,
                                    transform, playback_settings);

    TRACE_EVENT0("cc",
                 "RasterBufferProvider::PlaybackToMemory::ConvertRGBA4444");
    SkImageInfo dst_info =
        info.makeColorType(ToClosestSkColorType(gpu_compositing, format));
    auto dst_canvas =
        SkCanvas::MakeRasterDirect(dst_info, memory, stride, &surface_props);
    DCHECK(dst_canvas);
    SkPaint paint;
    paint.setDither(true);
    paint.setBlendMode(SkBlendMode::kSrc);
    surface->draw(dst_canvas.get(), 0, 0, SkSamplingOptions(), &paint);
    return;
  }

  NOTREACHED();
}

void RasterBufferProvider::FlushIfNeeded() {
  if (!needs_flush_) {
    return;
  }

  Flush();
  needs_flush_ = false;
}

}  // namespace cc
