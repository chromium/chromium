// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_DEBUG_COLORS_H_
#define CC_DEBUG_DEBUG_COLORS_H_

#include "base/containers/span.h"
#include "cc/debug/debug_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class CC_DEBUG_EXPORT DebugColors {
 public:
  DebugColors() = delete;

  static SkColor TiledContentLayerBorderColor();
  static int TiledContentLayerBorderWidth(float device_scale_factor);

  static SkColor ImageLayerBorderColor();
  static int ImageLayerBorderWidth(float device_scale_factor);

  static SkColor ContentLayerBorderColor();
  static int ContentLayerBorderWidth(float device_scale_factor);

  static SkColor MaskingLayerBorderColor();
  static int MaskingLayerBorderWidth(float device_scale_factor);

  static SkColor ContainerLayerBorderColor();
  static int ContainerLayerBorderWidth(float device_scale_factor);

  static SkColor SurfaceLayerBorderColor();
  static int SurfaceLayerBorderWidth(float device_scale_factor);

  static SkColor SurfaceBorderColor();
  static int SurfaceBorderWidth(float device_scale_factor);

  static SkColor HighResTileBorderColor();
  static int HighResTileBorderWidth(float device_scale_factor);

  static SkColor LowResTileBorderColor();
  static int LowResTileBorderWidth(float device_scale_factor);

  static SkColor ExtraHighResTileBorderColor();
  static int ExtraHighResTileBorderWidth(float device_scale_factor);

  static SkColor ExtraLowResTileBorderColor();
  static int ExtraLowResTileBorderWidth(float device_scale_factor);

  static SkColor MissingTileBorderColor();
  static int MissingTileBorderWidth(float device_scale_factor);

  static SkColor SolidColorTileBorderColor();
  static int SolidColorTileBorderWidth(float device_scale_factor);

  static SkColor OOMTileBorderColor();
  static int OOMTileBorderWidth(float device_scale_factor);

  static SkColor DirectPictureBorderColor();
  static int DirectPictureBorderWidth(float device_scale_factor);

  static SkColor CompressedTileBorderColor();
  static int CompressedTileBorderWidth(float device_scale_factor);

  static SkColor DefaultCheckerboardColor();
  static SkColor EvictedTileCheckerboardColor();
  static SkColor InvalidatedTileCheckerboardColor();

  static const int kFadeSteps = 50;
  static SkColor PaintRectBorderColor(int step);
  static int PaintRectBorderWidth();
  static SkColor PaintRectFillColor(int step);

  static SkColor LayoutShiftRectBorderColor();
  static int LayoutShiftRectBorderWidth();
  static SkColor LayoutShiftRectFillColor(int step);

  static SkColor PropertyChangedRectBorderColor();
  static int PropertyChangedRectBorderWidth();
  static SkColor PropertyChangedRectFillColor();

  static SkColor SurfaceDamageRectBorderColor();
  static int SurfaceDamageRectBorderWidth();
  static SkColor SurfaceDamageRectFillColor();

  static SkColor ScreenSpaceLayerRectBorderColor();
  static int ScreenSpaceLayerRectBorderWidth();
  static SkColor ScreenSpaceLayerRectFillColor();

  static SkColor TouchEventHandlerRectBorderColor();
  static int TouchEventHandlerRectBorderWidth();
  static SkColor TouchEventHandlerRectFillColor();

  static SkColor WheelEventHandlerRectBorderColor();
  static int WheelEventHandlerRectBorderWidth();
  static SkColor WheelEventHandlerRectFillColor();

  static SkColor ScrollEventHandlerRectBorderColor();
  static int ScrollEventHandlerRectBorderWidth();
  static SkColor ScrollEventHandlerRectFillColor();

  static SkColor NonFastScrollableRectBorderColor();
  static int NonFastScrollableRectBorderWidth();
  static SkColor NonFastScrollableRectFillColor();

  static SkColor MainThreadScrollingReasonRectBorderColor();
  static int MainThreadScrollingReasonRectBorderWidth();
  static SkColor MainThreadScrollingReasonRectFillColor();

  static SkColor LayerAnimationBoundsBorderColor();
  static int LayerAnimationBoundsBorderWidth();
  static SkColor LayerAnimationBoundsFillColor();

  static SkColor NonPaintedFillColor();
  static SkColor MissingPictureFillColor();
  static SkColor MissingResizeInvalidations();
  static SkColor PictureBorderColor();

  static base::span<const float> TintCompositedContentColorTransformMatrix();

  static SkColor HUDBackgroundColor();
  static SkColor HUDSeparatorLineColor();
  static SkColor HUDIndicatorLineColor();
  static SkColor HUDTitleColor();

  static SkColor PlatformLayerTreeTextColor();
  static SkColor FPSDisplayTextAndGraphColor();
  static SkColor MemoryDisplayTextColor();
  static SkColor PaintTimeDisplayTextAndGraphColor();
};

}  // namespace cc

#endif  // CC_DEBUG_DEBUG_COLORS_H_
