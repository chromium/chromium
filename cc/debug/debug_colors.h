// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_DEBUG_COLORS_H_
#define CC_DEBUG_DEBUG_COLORS_H_

#include "base/containers/span.h"
#include "cc/debug/debug_export.h"
#include "cc/raster/lcd_text_disallowed_reason.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class CC_DEBUG_EXPORT DebugColors {
 public:
  DebugColors() = delete;

  static SkColor4f TiledContentLayerBorderColor();
  static int TiledContentLayerBorderWidth(float device_scale_factor);

  static SkColor4f ImageLayerBorderColor();
  static int ImageLayerBorderWidth(float device_scale_factor);

  static SkColor4f ContentLayerBorderColor();
  static int ContentLayerBorderWidth(float device_scale_factor);

  static SkColor4f ContainerLayerBorderColor();
  static int ContainerLayerBorderWidth(float device_scale_factor);

  static SkColor4f SurfaceLayerBorderColor();
  static int SurfaceLayerBorderWidth(float device_scale_factor);

  static SkColor4f SurfaceBorderColor();
  static int SurfaceBorderWidth(float device_scale_factor);

  static SkColor4f HighResTileBorderColor();
  static int HighResTileBorderWidth(float device_scale_factor);

  static SkColor4f LowResTileBorderColor();
  static int LowResTileBorderWidth(float device_scale_factor);

  static SkColor4f ExtraHighResTileBorderColor();
  static int ExtraHighResTileBorderWidth(float device_scale_factor);

  static SkColor4f ExtraLowResTileBorderColor();
  static int ExtraLowResTileBorderWidth(float device_scale_factor);

  static SkColor4f MissingTileBorderColor();
  static int MissingTileBorderWidth(float device_scale_factor);

  static SkColor4f SolidColorTileBorderColor();
  static int SolidColorTileBorderWidth(float device_scale_factor);

  static SkColor4f OOMTileBorderColor();
  static int OOMTileBorderWidth(float device_scale_factor);

  static SkColor4f DirectPictureBorderColor();
  static int DirectPictureBorderWidth(float device_scale_factor);

  static SkColor4f CompressedTileBorderColor();
  static int CompressedTileBorderWidth(float device_scale_factor);

  static SkColor4f DefaultCheckerboardColor();
  static SkColor4f EvictedTileCheckerboardColor();
  static SkColor4f InvalidatedTileCheckerboardColor();

  static const int kFadeSteps = 50;
  static SkColor4f PaintRectBorderColor(int step);
  static int PaintRectBorderWidth();
  static SkColor4f PaintRectFillColor(int step);

  static SkColor4f LayoutShiftRectBorderColor();
  static int LayoutShiftRectBorderWidth();
  static SkColor4f LayoutShiftRectFillColor(int step);

  static SkColor4f PropertyChangedRectBorderColor();
  static int PropertyChangedRectBorderWidth();
  static SkColor4f PropertyChangedRectFillColor();

  static SkColor4f SurfaceDamageRectBorderColor();
  static int SurfaceDamageRectBorderWidth();
  static SkColor4f SurfaceDamageRectFillColor();

  static SkColor4f ScreenSpaceLayerRectBorderColor();
  static int ScreenSpaceLayerRectBorderWidth();
  static SkColor4f ScreenSpaceLayerRectFillColor();

  static SkColor4f TouchEventHandlerRectBorderColor();
  static int TouchEventHandlerRectBorderWidth();
  static SkColor4f TouchEventHandlerRectFillColor();

  static SkColor4f WheelEventHandlerRectBorderColor();
  static int WheelEventHandlerRectBorderWidth();
  static SkColor4f WheelEventHandlerRectFillColor();

  static SkColor4f ScrollEventHandlerRectBorderColor();
  static int ScrollEventHandlerRectBorderWidth();
  static SkColor4f ScrollEventHandlerRectFillColor();

  static SkColor4f MainThreadScrollHitTestRectBorderColor();
  static int MainThreadScrollHitTestRectBorderWidth();
  static SkColor4f MainThreadScrollHitTestRectFillColor();

  static SkColor4f MainThreadScrollRepaintRectBorderColor();
  static int MainThreadScrollRepaintRectBorderWidth();
  static SkColor4f MainThreadScrollRepaintRectFillColor();

  static SkColor4f RasterInducingScrollRectBorderColor();
  static int RasterInducingScrollRectBorderWidth();
  static SkColor4f RasterInducingScrollRectFillColor();

  static SkColor4f LayerAnimationBoundsBorderColor();
  static int LayerAnimationBoundsBorderWidth();
  static SkColor4f LayerAnimationBoundsFillColor();

  static SkColor4f NonPaintedFillColor();
  static SkColor4f MissingPictureFillColor();
  static SkColor4f MissingResizeInvalidations();
  static SkColor4f PictureBorderColor();

  static base::span<const float> TintCompositedContentColorTransformMatrix();

  static SkColor4f HUDBackgroundColor();
  static SkColor4f HUDSeparatorLineColor();
  static SkColor4f HUDIndicatorLineColor();
  static SkColor4f HUDTitleColor();

  static SkColor4f PlatformLayerTreeTextColor();
  static SkColor4f FPSDisplayTextAndGraphColor();
  static SkColor4f FPSDisplayDroppedFrame();
  static SkColor4f FPSDisplayMissedFrame();
  static SkColor4f FPSDisplaySuccessfulFrame();
  static SkColor4f MemoryDisplayTextColor();
  static SkColor4f PaintTimeDisplayTextAndGraphColor();

  static SkColor4f NonLCDTextHighlightColor(LCDTextDisallowedReason);
};

}  // namespace cc

#endif  // CC_DEBUG_DEBUG_COLORS_H_
