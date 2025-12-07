// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_region_overlay_controller.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/scanner/scanner_text.h"
#include "base/time/time.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/render_surface_filters.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

namespace {

// TODO(b/367548979): Use correct style constants for detected text regions.
constexpr SkColor kDetectedTextRegionColor = SK_ColorCYAN;
constexpr float kDetectedTextRegionOpacity = 0.3f;

// TODO(b/367549273): Use correct colors to paint translated text.
constexpr SkColor kTranslatedTextColor = SK_ColorBLACK;
constexpr SkColor kTranslatedTextBackgroundColor = SK_ColorWHITE;

// The duration of the region glow pulse animation.
constexpr base::TimeDelta kRegionGlowPulseDuration = base::Milliseconds(666);

// The minimum and maximum opacity of the region glow pulse animation.
constexpr float kRegionGlowMinOpacity = 0.35f;
constexpr float kRegionGlowMaxOpacity = 1.0f;

// Translates and rotates `canvas` so that `center_rotated_box` is upright and
// centered on the canvas. The components of `center_rotated_box` should be
// specified as coordinates relative to `region`.
void TranslateAndRotateCanvas(
    gfx::Canvas& canvas,
    const gfx::Rect& region,
    const ScannerText::CenterRotatedBox& center_rotated_box) {
  canvas.Translate(center_rotated_box.center.OffsetFromOrigin() +
                   region.origin().OffsetFromOrigin());
  canvas.sk_canvas()->rotate(center_rotated_box.rotation);
}

// Gets a rect with size `size` centered at the origin (offset to the nearest
// integer coordinates if `size` has odd width or odd height).
gfx::Rect GetRectCenteredAtOrigin(const gfx::Size& size) {
  return gfx::Rect(-size.width() / 2, -size.height() / 2, size.width(),
                   size.height());
}

}  // namespace

CaptureRegionOverlayController::CaptureRegionOverlayController() {
  DCHECK(CanShowSunfishOrScannerUi());
}

CaptureRegionOverlayController::~CaptureRegionOverlayController() = default;

void CaptureRegionOverlayController::OnTextDetected(
    std::optional<ScannerText> detected_text) {
  detected_text_ = std::move(detected_text);
}

void CaptureRegionOverlayController::OnTranslatedTextFetched(
    std::optional<ScannerText> translated_text) {
  translated_text_ = std::move(translated_text);
}

void CaptureRegionOverlayController::PaintCaptureRegionOverlay(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {
  PaintDetectedTextRegions(canvas, region_bounds_in_canvas);
  PaintTranslatedText(canvas, region_bounds_in_canvas);
}

void CaptureRegionOverlayController::StartGlowAnimation(
    gfx::AnimationDelegate* animation_delegate) {
  if (!glow_animation_) {
    glow_animation_ = std::make_unique<gfx::ThrobAnimation>(animation_delegate);
    glow_animation_->SetThrobDuration(kRegionGlowPulseDuration);
    glow_animation_->SetTweenType(gfx::Tween::LINEAR);
  }
  // Set `cycles_til_stop` to be negative so that the animation continues
  // indefinitely.
  glow_animation_->StartThrobbing(/*cycles_til_stop=*/-1);
}

void CaptureRegionOverlayController::PauseGlowAnimation() {
  if (glow_animation_) {
    // Complete the current animation cycle then remain there. This will pause
    // the glow animation at the end of the cycle, where the glow has minimum
    // outset and blur.
    glow_animation_->set_cycles_remaining(0);
  }
}

void CaptureRegionOverlayController::RemoveGlowAnimation() {
  glow_animation_ = nullptr;
}

bool CaptureRegionOverlayController::HasGlowAnimation() const {
  return glow_animation_ != nullptr;
}

void CaptureRegionOverlayController::PaintCurrentGlowState(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas,
    const ui::ColorProvider* color_provider) const {
  if (!glow_animation_) {
    return;
  }

  gfx::Rect current_glow_bounds(region_bounds_in_canvas);
  current_glow_bounds.Outset(glow_animation_->CurrentValueBetween(
      capture_mode::kRegionGlowMinOutsetDp,
      capture_mode::kRegionGlowMaxOutsetDp));
  cc::PaintFlags flags;
  flags.setAlphaf(static_cast<float>(glow_animation_->CurrentValueBetween(
      kRegionGlowMinOpacity, kRegionGlowMaxOpacity)));
  flags.setShader(gfx::CreateGradientShader(
      current_glow_bounds.origin(), current_glow_bounds.top_right(),
      color_provider->GetColor(cros_tokens::kCrosSysMuted),
      color_provider->GetColor(cros_tokens::kCrosSysComplement)));
  flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(
          glow_animation_->CurrentValueBetween(
              capture_mode::kRegionGlowAnimationMinBlurDp,
              capture_mode::kRegionGlowAnimationMaxBlurDp))})));
  canvas.DrawRect(current_glow_bounds, flags);
}

void CaptureRegionOverlayController::PaintDetectedTextRegions(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {
  if (!detected_text_.has_value()) {
    return;
  }

  // TODO(b/367548979): Paint detected text regions with correct styling.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kDetectedTextRegionColor);
  flags.setAlphaf(kDetectedTextRegionOpacity);
  for (const ScannerText::Paragraph& paragraph : detected_text_->paragraphs()) {
    for (const ScannerText::Line& line : paragraph.lines()) {
      canvas.Save();
      TranslateAndRotateCanvas(canvas, region_bounds_in_canvas,
                               line.bounding_box());
      canvas.DrawRect(GetRectCenteredAtOrigin(line.bounding_box().size), flags);
      canvas.Restore();
    }
  }
}

void CaptureRegionOverlayController::PaintTranslatedText(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {
  if (!translated_text_.has_value()) {
    return;
  }

  // TODO(b/367549273): Paint translated text with correct styling.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kTranslatedTextBackgroundColor);
  for (const ScannerText::Paragraph& paragraph :
       translated_text_->paragraphs()) {
    for (const ScannerText::Line& line : paragraph.lines()) {
      canvas.Save();
      TranslateAndRotateCanvas(canvas, region_bounds_in_canvas,
                               line.bounding_box());
      const gfx::Rect centered_bounds =
          GetRectCenteredAtOrigin(line.bounding_box().size);
      canvas.DrawRect(centered_bounds, flags);
      canvas.DrawStringRect(translated_text_->GetTextFromRange(line.range()),
                            gfx::FontList(), kTranslatedTextColor,
                            centered_bounds);
      canvas.Restore();
    }
  }
}

}  // namespace ash
