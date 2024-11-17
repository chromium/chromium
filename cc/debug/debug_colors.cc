// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/debug_colors.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace cc {

static float Scale(float width, float device_scale_factor) {
  return width * device_scale_factor;
}

// ======= Layer border colors =======

// Tiled content layers are orange.
SkColor4f DebugColors::TiledContentLayerBorderColor() {
  return {1.0f, 0.5f, 0.0f, 0.5f};
}
int DebugColors::TiledContentLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Image layers are olive.
SkColor4f DebugColors::ImageLayerBorderColor() {
  return {0.5f, 0.5f, 0.0f, 0.5f};
}
int DebugColors::ImageLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Non-tiled content layers area green.
SkColor4f DebugColors::ContentLayerBorderColor() {
  return {0.0f, 0.5f, 32.0f / 255.0f, 0.5f};
}
int DebugColors::ContentLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Other container layers are yellow.
SkColor4f DebugColors::ContainerLayerBorderColor() {
  return {1.0f, 1.0f, 0.0f, 0.75f};
}
int DebugColors::ContainerLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Surface layers are a blue-ish green.
SkColor4f DebugColors::SurfaceLayerBorderColor() {
  return {0.0f, 1.0f, 136.0f / 255.0f, 0.5f};
}
int DebugColors::SurfaceLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Render surfaces are blue.
SkColor4f DebugColors::SurfaceBorderColor() {
  return {0.0f, 0.0f, 1.0f, 100.0f / 255.0f};
}
int DebugColors::SurfaceBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// ======= Tile colors =======

// High-res tile borders are cyan.
SkColor4f DebugColors::HighResTileBorderColor() {
  return {80.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 100.0f / 255.0f};
}
int DebugColors::HighResTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Low-res tile borders are purple.
SkColor4f DebugColors::LowResTileBorderColor() {
  return {212.0f / 255.0f, 83.0f / 255.0f, 0.75f, 100.0f / 255.0f};
}
int DebugColors::LowResTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Other high-resolution tile borders are yellow.
SkColor4f DebugColors::ExtraHighResTileBorderColor() {
  return {239.0f / 255.0f, 231.0f / 255.0f, 20.0f / 255.0f, 100.0f / 255.0f};
}
int DebugColors::ExtraHighResTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Other low-resolution tile borders are green.
SkColor4f DebugColors::ExtraLowResTileBorderColor() {
  return {93.0f / 255.0f, 186.0f / 255.0f, 18.0f / 255.0f, 100.0f / 255.0f};
}
int DebugColors::ExtraLowResTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Missing tile borders are dark grey.
SkColor4f DebugColors::MissingTileBorderColor() {
  return {0.25f, 0.25f, 0.0f, 0.25f};
}
int DebugColors::MissingTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Solid color tile borders are grey.
SkColor4f DebugColors::SolidColorTileBorderColor() {
  return {0.5f, 0.5f, 0.5f, 0.5f};
}
int DebugColors::SolidColorTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// OOM tile borders are red.
SkColor4f DebugColors::OOMTileBorderColor() {
  return {1.0f, 0.0f, 0.0f, 100.0f / 255.0f};
}
int DebugColors::OOMTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Direct picture borders are chartreuse.
SkColor4f DebugColors::DirectPictureBorderColor() {
  return {127.0f / 255.0f, 1.0f, 0.0f, 1.0f};
}
int DebugColors::DirectPictureBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Returns a color transform that shifts color toward red.
base::span<const float>
DebugColors::TintCompositedContentColorTransformMatrix() {
  // The new colors are:
  // new_R = R + 0.3 G + 0.3 B
  // new_G =     0.7 G
  // new_B =             0.7 B
  // clang-format off
  static constexpr auto kColorTransform = std::to_array<float>({
                                              1.0f, 0.0f, 0.0f, 0.0f,
                                              0.3f, 0.7f, 0.0f, 0.0f,
                                              0.3f, 0.0f, 0.7f, 0.0f,
                                              0.0f, 0.0f, 0.0f, 1.0f});
  // clang-format on
  return base::span<const float>(kColorTransform);
}

// Compressed tile borders are blue.
SkColor4f DebugColors::CompressedTileBorderColor() {
  return {20.0f / 255.0f, 20.0f / 255.0f, 240.0f / 255.0f, 100.0f / 255.0f};
}
int DebugColors::CompressedTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// ======= Checkerboard colors =======

// Non-debug checkerboards are grey.
SkColor4f DebugColors::DefaultCheckerboardColor() {
  return {241.0f / 255.0f, 241.0f / 255.0f, 241.0f / 255.0f, 1.0f};
}

// Invalidated tiles get sky blue checkerboards.
SkColor4f DebugColors::InvalidatedTileCheckerboardColor() {
  return {0.5f, 200.0f / 255.0f, 245.0f / 255.0f, 1.0f};
}

// Evicted tiles get pale red checkerboards.
SkColor4f DebugColors::EvictedTileCheckerboardColor() {
  return {1.0f, 200.0f / 255.0f, 200.0f / 255.0f, 1.0f};
}

// ======= Debug rect colors =======

static SkColor4f FadedGreen(int initial_value, int step) {
  DCHECK_GE(step, 0);
  DCHECK_LE(step, DebugColors::kFadeSteps);
  int value = step * initial_value / DebugColors::kFadeSteps;
  return {0.0f, 195.0f / 255.0f, 0.0f, static_cast<float>(value) / 255.0f};
}
// Paint rects in green.
SkColor4f DebugColors::PaintRectBorderColor(int step) {
  return FadedGreen(255, step);
}
int DebugColors::PaintRectBorderWidth() { return 2; }
SkColor4f DebugColors::PaintRectFillColor(int step) {
  return FadedGreen(60, step);
}

static SkColor4f FadedBlue(int initial_value, int step) {
  DCHECK_GE(step, 0);
  DCHECK_LE(step, DebugColors::kFadeSteps);
  int value = step * initial_value / DebugColors::kFadeSteps;
  return {0.0f, 0.0f, 1.0f, static_cast<float>(value) / 255.0f};
}
/// Layout Shift rects in blue.
SkColor4f DebugColors::LayoutShiftRectBorderColor() {
  return {0.0f, 0.0f, 1.0f, 0.0f};
}
int DebugColors::LayoutShiftRectBorderWidth() {
  // We don't want any border showing for the layout shift debug rects so we set
  // the border width to be equal to 0.
  return 0;
}
SkColor4f DebugColors::LayoutShiftRectFillColor(int step) {
  return FadedBlue(60, step);
}

// Property-changed rects in blue.
SkColor4f DebugColors::PropertyChangedRectBorderColor() {
  return {0.0f, 0.0f, 1.0f, 1.0f};
}
int DebugColors::PropertyChangedRectBorderWidth() { return 2; }
SkColor4f DebugColors::PropertyChangedRectFillColor() {
  return {0.0f, 0.0f, 1.0f, 30.0f / 255.0f};
}

// Surface damage rects in yellow-orange.
SkColor4f DebugColors::SurfaceDamageRectBorderColor() {
  return {200.0f / 255.0f, 100.0f / 255.0f, 0.0f, 1.0f};
}
int DebugColors::SurfaceDamageRectBorderWidth() { return 2; }
SkColor4f DebugColors::SurfaceDamageRectFillColor() {
  return {200.0f / 255.0f, 100.0f / 255.0f, 0.0f, 30.0f / 255.0f};
}

// Surface screen space rects in yellow-green.
SkColor4f DebugColors::ScreenSpaceLayerRectBorderColor() {
  return {100.0f / 255.0f, 200.0f / 255.0f, 0.0f, 1.0f};
}
int DebugColors::ScreenSpaceLayerRectBorderWidth() { return 2; }
SkColor4f DebugColors::ScreenSpaceLayerRectFillColor() {
  return {100.0f / 255.0f, 200.0f / 255.0f, 0.0f, 30.0f / 255.0f};
}

// Touch-event-handler rects in yellow.
SkColor4f DebugColors::TouchEventHandlerRectBorderColor() {
  return {239.0f / 255.0f, 229.0f / 255.0f, 60.0f / 255.0f, 1.0f};
}
int DebugColors::TouchEventHandlerRectBorderWidth() { return 2; }
SkColor4f DebugColors::TouchEventHandlerRectFillColor() {
  return {239.0f / 255.0f, 229.0f / 255.0f, 60.0f / 255.0f, 30.0f / 255.0f};
}

// Wheel-event-handler rects in green.
SkColor4f DebugColors::WheelEventHandlerRectBorderColor() {
  return {189.0f / 255.0f, 209.0f / 255.0f, 57.0f / 255.0f, 1.0f};
}
int DebugColors::WheelEventHandlerRectBorderWidth() { return 2; }
SkColor4f DebugColors::WheelEventHandlerRectFillColor() {
  return {189.0f / 255.0f, 209.0f / 255.0f, 57.0f / 255.0f, 30.0f / 255.0f};
}

// Scroll-event-handler rects in teal.
SkColor4f DebugColors::ScrollEventHandlerRectBorderColor() {
  return {24.0f / 255.0f, 167.0f / 255.0f, 181.0f / 255.0f, 1.0f};
}
int DebugColors::ScrollEventHandlerRectBorderWidth() { return 2; }
SkColor4f DebugColors::ScrollEventHandlerRectFillColor() {
  return {24.0f / 255.0f, 167.0f / 255.0f, 181.0f / 255.0f, 30.0f / 255.0f};
}

// Main-thread scroll hit-test rects in orange.
SkColor4f DebugColors::MainThreadScrollHitTestRectBorderColor() {
  return {238.0f / 255.0f, 163.0f / 255.0f, 59.0f / 255.0f, 1.0f};
}
int DebugColors::MainThreadScrollHitTestRectBorderWidth() {
  return 2;
}
SkColor4f DebugColors::MainThreadScrollHitTestRectFillColor() {
  return {238.0f / 255.0f, 163.0f / 255.0f, 59.0f / 255.0f, 30.0f / 255.0f};
}

// Main-thread scroll repaint rects in yellow-orange.
SkColor4f DebugColors::MainThreadScrollRepaintRectBorderColor() {
  return {200.0f / 255.0f, 100.0f / 255.0f, 0.0f, 1.0f};
}
int DebugColors::MainThreadScrollRepaintRectBorderWidth() {
  return 2;
}
SkColor4f DebugColors::MainThreadScrollRepaintRectFillColor() {
  return {200.0f / 255.0f, 100.0f / 255.0f, 0.0f, 30.0f / 255.0f};
}

// Raster-inducing scroll rects in light yellow-orange.
SkColor4f DebugColors::RasterInducingScrollRectBorderColor() {
  return {200.0f / 255.0f, 100.0f / 255.0f, 0.0f, 0.5f};
}
int DebugColors::RasterInducingScrollRectBorderWidth() {
  return 2;
}
SkColor4f DebugColors::RasterInducingScrollRectFillColor() {
  return {200.0f / 255.0f, 100.0f / 255.0f, 0.0f, 15.0f / 255.0f};
}

// Animation bounds are lime-green.
SkColor4f DebugColors::LayerAnimationBoundsBorderColor() {
  return {112.0f / 255.0f, 229.0f / 255.0f, 0.0f, 1.0f};
}
int DebugColors::LayerAnimationBoundsBorderWidth() { return 2; }
SkColor4f DebugColors::LayerAnimationBoundsFillColor() {
  return {112.0f / 255.0f, 229.0f / 255.0f, 0.0f, 30.0f / 255.0f};
}

// Picture borders in transparent blue.
SkColor4f DebugColors::PictureBorderColor() {
  return {0.0f, 0.0f, 200.0f / 255.0f, 100.0f / 255.0f};
}

// ======= HUD widget colors =======

SkColor4f DebugColors::HUDBackgroundColor() {
  return {0.0f, 0.0f, 0.0f, 217.0f / 255.0f};
}
SkColor4f DebugColors::HUDSeparatorLineColor() {
  return {0.0f, 1.0f, 0.0f, 0.25f};
}
SkColor4f DebugColors::HUDIndicatorLineColor() {
  return SkColors::kYellow;
}
SkColor4f DebugColors::HUDTitleColor() {
  return {232.0f / 255.0f, 232.0f / 255.0f, 232.0f / 255.0f, 1.0f};
}

SkColor4f DebugColors::PlatformLayerTreeTextColor() {
  return SkColors::kRed;
}
SkColor4f DebugColors::FPSDisplayTextAndGraphColor() {
  return SkColors::kGreen;
}

// Color used to represent dropped compositor frames.
SkColor4f DebugColors::FPSDisplayDroppedFrame() {
  return {202.0f / 255.0f, 91.0f / 255.0f, 29.0f / 255.0f, 1.0f};
}
// Color used to represent a "partial" frame, i.e. a frame that missed
// its commit deadline.
SkColor4f DebugColors::FPSDisplayMissedFrame() {
  return {1.0f, 245.0f / 255.0f, 0.0f, 1.0f};
}
// Color used to represent a frame that successfully rendered.
SkColor4f DebugColors::FPSDisplaySuccessfulFrame() {
  return {174.0f / 255.0f, 221.0f / 255.0f, 1.0f, 191.0f / 255.0f};
}

SkColor4f DebugColors::MemoryDisplayTextColor() {
  return SkColors::kCyan;
}

// Paint time display in green (similar to paint times in the WebInspector)
SkColor4f DebugColors::PaintTimeDisplayTextAndGraphColor() {
  return {75.0f / 255.0f, 155.0f / 255.0f, 55.0f / 255.0f, 1.0f};
}

SkColor4f DebugColors::NonLCDTextHighlightColor(
    LCDTextDisallowedReason reason) {
  switch (reason) {
    case LCDTextDisallowedReason::kNone:
    case LCDTextDisallowedReason::kNoText:
      return SkColors::kTransparent;
    case LCDTextDisallowedReason::kSetting:
      return {0.5f, 1.0f, 0.0f, 96.0f / 255.0f};
    case LCDTextDisallowedReason::kBackgroundColorNotOpaque:
      return {0.5f, 0.5f, 0.0f, 96.0f / 255.0f};
    case LCDTextDisallowedReason::kContentsNotOpaque:
      return {1.0f, 0.0f, 0.0f, 96.0f / 255.0f};
    case LCDTextDisallowedReason::kNonIntegralTranslation:
      return {1.0f, 0.5f, 0.0f, 96.0f / 255.0f};
    case LCDTextDisallowedReason::kNonIntegralXOffset:
    case LCDTextDisallowedReason::kNonIntegralYOffset:
      return {1.0f, 0.0f, 0.5f, 96.0f / 255.0f};
    case LCDTextDisallowedReason::kWillChangeTransform:
    case LCDTextDisallowedReason::kTransformAnimation:
      return {0.5f, 0.0f, 1.0f, 96.0f / 255.0f};
    case LCDTextDisallowedReason::kPixelOrColorEffect:
      return {0.0f, 0.5f, 0.0f, 96.0f / 255.0f};
  }
  NOTREACHED();
}

}  // namespace cc
