// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "cc/debug/debug_colors.h"

namespace cc {

static float Scale(float width, float device_scale_factor) {
  return width * device_scale_factor;
}

// ======= Layer border colors =======

// Tiled content layers are orange.
SkColor DebugColors::TiledContentLayerBorderColor() {
  return SkColorSetARGB(128, 255, 128, 0);
}
int DebugColors::TiledContentLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Image layers are olive.
SkColor DebugColors::ImageLayerBorderColor() {
  return SkColorSetARGB(128, 128, 128, 0);
}
int DebugColors::ImageLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Non-tiled content layers area green.
SkColor DebugColors::ContentLayerBorderColor() {
  return SkColorSetARGB(128, 0, 128, 32);
}
int DebugColors::ContentLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Masking layers are pale blue and wide.
SkColor DebugColors::MaskingLayerBorderColor() {
  return SkColorSetARGB(48, 128, 255, 255);
}
int DebugColors::MaskingLayerBorderWidth(float device_scale_factor) {
  return Scale(20, device_scale_factor);
}

// Other container layers are yellow.
SkColor DebugColors::ContainerLayerBorderColor() {
  return SkColorSetARGB(192, 255, 255, 0);
}
int DebugColors::ContainerLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Surface layers are a blue-ish green.
SkColor DebugColors::SurfaceLayerBorderColor() {
  return SkColorSetARGB(128, 0, 255, 136);
}
int DebugColors::SurfaceLayerBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Render surfaces are blue.
SkColor DebugColors::SurfaceBorderColor() {
  return SkColorSetARGB(100, 0, 0, 255);
}
int DebugColors::SurfaceBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// ======= Tile colors =======

// High-res tile borders are cyan.
SkColor DebugColors::HighResTileBorderColor() {
  return SkColorSetARGB(100, 80, 200, 200);
}
int DebugColors::HighResTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Low-res tile borders are purple.
SkColor DebugColors::LowResTileBorderColor() {
  return SkColorSetARGB(100, 212, 83, 192);
}
int DebugColors::LowResTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Other high-resolution tile borders are yellow.
SkColor DebugColors::ExtraHighResTileBorderColor() {
  return SkColorSetARGB(100, 239, 231, 20);
}
int DebugColors::ExtraHighResTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Other low-resolution tile borders are green.
SkColor DebugColors::ExtraLowResTileBorderColor() {
  return SkColorSetARGB(100, 93, 186, 18);
}
int DebugColors::ExtraLowResTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// Missing tile borders are dark grey.
SkColor DebugColors::MissingTileBorderColor() {
  return SkColorSetARGB(64, 64, 64, 0);
}
int DebugColors::MissingTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Solid color tile borders are grey.
SkColor DebugColors::SolidColorTileBorderColor() {
  return SkColorSetARGB(128, 128, 128, 128);
}
int DebugColors::SolidColorTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// OOM tile borders are red.
SkColor DebugColors::OOMTileBorderColor() {
  return SkColorSetARGB(100, 255, 0, 0);
}
int DebugColors::OOMTileBorderWidth(float device_scale_factor) {
  return Scale(1, device_scale_factor);
}

// Direct picture borders are chartreuse.
SkColor DebugColors::DirectPictureBorderColor() {
  return SkColorSetARGB(255, 127, 255, 0);
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
  static constexpr float kColorTransform[] = {1.0f, 0.0f, 0.0f, 0.0f,
                                              0.3f, 0.7f, 0.0f, 0.0f,
                                              0.3f, 0.0f, 0.7f, 0.0f,
                                              0.0f, 0.0f, 0.0f, 1.0f};
  // clang-format on
  return base::span<const float>(kColorTransform, sizeof(kColorTransform));
}

// Compressed tile borders are blue.
SkColor DebugColors::CompressedTileBorderColor() {
  return SkColorSetARGB(100, 20, 20, 240);
}
int DebugColors::CompressedTileBorderWidth(float device_scale_factor) {
  return Scale(2, device_scale_factor);
}

// ======= Checkerboard colors =======

// Non-debug checkerboards are grey.
SkColor DebugColors::DefaultCheckerboardColor() {
  return SkColorSetRGB(241, 241, 241);
}

// Invalidated tiles get sky blue checkerboards.
SkColor DebugColors::InvalidatedTileCheckerboardColor() {
  return SkColorSetRGB(128, 200, 245);
}

// Evicted tiles get pale red checkerboards.
SkColor DebugColors::EvictedTileCheckerboardColor() {
  return SkColorSetRGB(255, 200, 200);
}

// ======= Debug rect colors =======

static SkColor FadedGreen(int initial_value, int step) {
  DCHECK_GE(step, 0);
  DCHECK_LE(step, DebugColors::kFadeSteps);
  int value = step * initial_value / DebugColors::kFadeSteps;
  return SkColorSetARGB(value, 0, 195, 0);
}
// Paint rects in green.
SkColor DebugColors::PaintRectBorderColor(int step) {
  return FadedGreen(255, step);
}
int DebugColors::PaintRectBorderWidth() { return 2; }
SkColor DebugColors::PaintRectFillColor(int step) {
  return FadedGreen(60, step);
}

static SkColor FadedBlue(int initial_value, int step) {
  DCHECK_GE(step, 0);
  DCHECK_LE(step, DebugColors::kFadeSteps);
  int value = step * initial_value / DebugColors::kFadeSteps;
  return SkColorSetARGB(value, 0, 0, 255);
}
/// Layout Shift rects in blue.
SkColor DebugColors::LayoutShiftRectBorderColor() {
  return SkColorSetARGB(0, 0, 0, 255);
}
int DebugColors::LayoutShiftRectBorderWidth() {
  // We don't want any border showing for the layout shift debug rects so we set
  // the border width to be equal to 0.
  return 0;
}
SkColor DebugColors::LayoutShiftRectFillColor(int step) {
  return FadedBlue(60, step);
}

// Property-changed rects in blue.
SkColor DebugColors::PropertyChangedRectBorderColor() {
  return SkColorSetARGB(255, 0, 0, 255);
}
int DebugColors::PropertyChangedRectBorderWidth() { return 2; }
SkColor DebugColors::PropertyChangedRectFillColor() {
  return SkColorSetARGB(30, 0, 0, 255);
}

// Surface damage rects in yellow-orange.
SkColor DebugColors::SurfaceDamageRectBorderColor() {
  return SkColorSetARGB(255, 200, 100, 0);
}
int DebugColors::SurfaceDamageRectBorderWidth() { return 2; }
SkColor DebugColors::SurfaceDamageRectFillColor() {
  return SkColorSetARGB(30, 200, 100, 0);
}

// Surface screen space rects in yellow-green.
SkColor DebugColors::ScreenSpaceLayerRectBorderColor() {
  return SkColorSetARGB(255, 100, 200, 0);
}
int DebugColors::ScreenSpaceLayerRectBorderWidth() { return 2; }
SkColor DebugColors::ScreenSpaceLayerRectFillColor() {
  return SkColorSetARGB(30, 100, 200, 0);
}

// Touch-event-handler rects in yellow.
SkColor DebugColors::TouchEventHandlerRectBorderColor() {
  return SkColorSetARGB(255, 239, 229, 60);
}
int DebugColors::TouchEventHandlerRectBorderWidth() { return 2; }
SkColor DebugColors::TouchEventHandlerRectFillColor() {
  return SkColorSetARGB(30, 239, 229, 60);
}

// Wheel-event-handler rects in green.
SkColor DebugColors::WheelEventHandlerRectBorderColor() {
  return SkColorSetARGB(255, 189, 209, 57);
}
int DebugColors::WheelEventHandlerRectBorderWidth() { return 2; }
SkColor DebugColors::WheelEventHandlerRectFillColor() {
  return SkColorSetARGB(30, 189, 209, 57);
}

// Scroll-event-handler rects in teal.
SkColor DebugColors::ScrollEventHandlerRectBorderColor() {
  return SkColorSetARGB(255, 24, 167, 181);
}
int DebugColors::ScrollEventHandlerRectBorderWidth() { return 2; }
SkColor DebugColors::ScrollEventHandlerRectFillColor() {
  return SkColorSetARGB(30, 24, 167, 181);
}

// Non-fast-scrollable rects in orange.
SkColor DebugColors::NonFastScrollableRectBorderColor() {
  return SkColorSetARGB(255, 238, 163, 59);
}
int DebugColors::NonFastScrollableRectBorderWidth() { return 2; }
SkColor DebugColors::NonFastScrollableRectFillColor() {
  return SkColorSetARGB(30, 238, 163, 59);
}

// Main-thread scrolling reason rects in yellow-orange.
SkColor DebugColors::MainThreadScrollingReasonRectBorderColor() {
  return SkColorSetARGB(255, 200, 100, 0);
}
int DebugColors::MainThreadScrollingReasonRectBorderWidth() {
  return 2;
}
SkColor DebugColors::MainThreadScrollingReasonRectFillColor() {
  return SkColorSetARGB(30, 200, 100, 0);
}

// Animation bounds are lime-green.
SkColor DebugColors::LayerAnimationBoundsBorderColor() {
  return SkColorSetARGB(255, 112, 229, 0);
}
int DebugColors::LayerAnimationBoundsBorderWidth() { return 2; }
SkColor DebugColors::LayerAnimationBoundsFillColor() {
  return SkColorSetARGB(30, 112, 229, 0);
}

// Picture borders in transparent blue.
SkColor DebugColors::PictureBorderColor() {
  return SkColorSetARGB(100, 0, 0, 200);
}

// ======= HUD widget colors =======

SkColor DebugColors::HUDBackgroundColor() {
  return SkColorSetARGB(208, 17, 17, 17);
}
SkColor DebugColors::HUDSeparatorLineColor() {
  return SkColorSetARGB(64, 0, 255, 0);
}
SkColor DebugColors::HUDIndicatorLineColor() {
  return SK_ColorYELLOW;
}
SkColor DebugColors::HUDTitleColor() {
  return SkColorSetARGB(255, 232, 232, 232);
}

SkColor DebugColors::PlatformLayerTreeTextColor() { return SK_ColorRED; }
SkColor DebugColors::FPSDisplayTextAndGraphColor() {
  return SK_ColorGREEN;
}
SkColor DebugColors::MemoryDisplayTextColor() {
  return SK_ColorCYAN;
}

// Paint time display in green (similar to paint times in the WebInspector)
SkColor DebugColors::PaintTimeDisplayTextAndGraphColor() {
  return SkColorSetRGB(75, 155, 55);
}

}  // namespace cc
