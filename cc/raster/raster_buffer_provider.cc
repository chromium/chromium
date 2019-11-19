// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "cc/raster/raster_source.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMath.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/axis_transform2d.h"

namespace cc {

RasterBufferProvider::RasterBufferProvider() = default;

RasterBufferProvider::~RasterBufferProvider() = default;

namespace {

bool IsSupportedPlaybackToMemoryFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RGBA_4444:
    case viz::RGBA_8888:
    case viz::BGRA_8888:
      return true;
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
    case viz::RGB_565:
    case viz::ETC1:
    case viz::RED_8:
    case viz::LUMINANCE_F16:
    case viz::RGBA_F16:
    case viz::R16_EXT:
    case viz::BGR_565:
    case viz::RG_88:
    case viz::RGBX_8888:
    case viz::BGRX_8888:
    case viz::RGBX_1010102:
    case viz::BGRX_1010102:
    case viz::YVU_420:
    case viz::YUV_420_BIPLANAR:
    case viz::P010:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // anonymous namespace

// static
void RasterBufferProvider::PlaybackToMemory(
    void* memory,
    viz::ResourceFormat format,
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

  DCHECK(IsSupportedPlaybackToMemoryFormat(format)) << format;

  SkColorType color_type =
      ResourceFormatToClosestSkColorType(gpu_compositing, format);

  // Uses kPremul_SkAlphaType since the result is not known to be opaque.
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kPremul_SkAlphaType,
                                       target_color_space.ToSkColorSpace());

  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  if (playback_settings.use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props = SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);
  }

  if (!stride)
    stride = info.minRowBytes();
  DCHECK_GT(stride, 0u);

  gfx::Size content_size = raster_source->GetContentSize(transform.scale());

  switch (format) {
    case viz::RGBA_8888:
    case viz::BGRA_8888:
    case viz::RGBA_F16: {
      sk_sp<SkSurface> surface =
          SkSurface::MakeRasterDirect(info, memory, stride, &surface_props);
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
    case viz::RGBA_4444: {
      sk_sp<SkSurface> surface = SkSurface::MakeRaster(info, &surface_props);
      // TODO(reveman): Improve partial raster support by reducing the size of
      // playback rect passed to PlaybackToCanvas. crbug.com/519070
      raster_source->PlaybackToCanvas(surface->getCanvas(), content_size,
                                      canvas_bitmap_rect, canvas_bitmap_rect,
                                      transform, playback_settings);

      TRACE_EVENT0("cc",
                   "RasterBufferProvider::PlaybackToMemory::ConvertRGBA4444");
      SkImageInfo dst_info = info.makeColorType(
          ResourceFormatToClosestSkColorType(gpu_compositing, format));
      auto dst_canvas = SkCanvas::MakeRasterDirect(dst_info, memory, stride);
      DCHECK(dst_canvas);
      SkPaint paint;
      paint.setDither(true);
      paint.setBlendMode(SkBlendMode::kSrc);
      surface->draw(dst_canvas.get(), 0, 0, &paint);
      return;
    }
    case viz::ETC1:
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
    case viz::RGB_565:
    case viz::RED_8:
    case viz::LUMINANCE_F16:
    case viz::R16_EXT:
    case viz::BGR_565:
    case viz::RG_88:
    case viz::RGBX_8888:
    case viz::BGRX_8888:
    case viz::RGBX_1010102:
    case viz::BGRX_1010102:
    case viz::YVU_420:
    case viz::YUV_420_BIPLANAR:
    case viz::P010:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

}  // namespace cc
