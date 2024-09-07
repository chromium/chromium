// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The number of density-indpendent pixels in the color gradient that fades to
// transparent.
constexpr int kGradientWidth = 6;
constexpr int kDefaultStrokeWidth = 2;
constexpr float kDashLengthDip = 4.f;
constexpr float kGapLengthDip = 2.f;

int sign(int x) {
  return ((x > 0) ? 1 : (x == 0) ? 0 : -1);
}

SkPath MakePath(const AccessibilityFocusRing& input_ring,
                int outset,
                const gfx::Vector2d& offset) {
  AccessibilityFocusRing ring = input_ring;

  for (int i = 0; i < 36; i++) {
    gfx::Point p = input_ring.points[i];
    gfx::Point prev;
    gfx::Point next;

    int prev_index = i;
    do {
      prev_index = (prev_index + 35) % 36;
      prev = input_ring.points[prev_index];
    } while (prev.x() == p.x() && prev.y() == p.y() && prev_index != i);

    int next_index = i;
    do {
      next_index = (next_index + 1) % 36;
      next = input_ring.points[next_index];
    } while (next.x() == p.x() && next.y() == p.y() && next_index != i);

    gfx::Point delta0 =
        gfx::Point(sign(p.x() - prev.x()), sign(p.y() - prev.y()));
    gfx::Point delta1 =
        gfx::Point(sign(next.x() - p.x()), sign(next.y() - p.y()));

    if (delta0.x() == delta1.x() && delta0.y() == delta1.y()) {
      ring.points[i] =
          gfx::Point(input_ring.points[i].x() + outset * delta0.y(),
                     input_ring.points[i].y() - outset * delta0.x());
    } else {
      ring.points[i] = gfx::Point(
          input_ring.points[i].x() + ((i + 13) % 36 >= 18 ? outset : -outset),
          input_ring.points[i].y() + ((i + 4) % 36 >= 18 ? outset : -outset));
    }
  }

  SkPath path;
  gfx::Point p = ring.points[0] - offset;
  path.moveTo(SkIntToScalar(p.x()), SkIntToScalar(p.y()));
  for (int i = 0; i < 12; i++) {
    int index0 = ((3 * i) + 1) % 36;
    int index1 = ((3 * i) + 2) % 36;
    int index2 = ((3 * i) + 3) % 36;
    gfx::Point p0 = ring.points[index0] - offset;
    gfx::Point p1 = ring.points[index1] - offset;
    gfx::Point p2 = ring.points[index2] - offset;
    path.lineTo(SkIntToScalar(p0.x()), SkIntToScalar(p0.y()));
    path.quadTo(SkIntToScalar(p1.x()), SkIntToScalar(p1.y()),
                SkIntToScalar(p2.x()), SkIntToScalar(p2.y()));
  }

  return path;
}

}  // namespace

AccessibilityFocusRingLayer::AccessibilityFocusRingLayer(
    AccessibilityLayerDelegate* delegate)
    : FocusRingLayer(delegate) {}

AccessibilityFocusRingLayer::~AccessibilityFocusRingLayer() = default;

void AccessibilityFocusRingLayer::Set(const AccessibilityFocusRing& ring) {
  ring_ = ring;

  gfx::Rect bounds = ring.GetBounds();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  aura::Window* container = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityBubbleContainer);

  if (SkColorGetA(background_color_) > 0) {
    bounds = display.bounds();
  } else {
    int inset = kGradientWidth;
    bounds.Inset(-inset);
  }
  ::wm::ConvertRectFromScreen(container, &bounds);
  bool stack_at_top =
      (stacking_order_ == FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES);
  CreateOrUpdateLayer(container, "AccessibilityFocusRing", bounds,
                      stack_at_top);
}

void AccessibilityFocusRingLayer::SetAppearance(
    FocusRingType type,
    FocusRingStackingOrder stacking_order,
    SkColor color,
    SkColor secondary_color,
    SkColor background_color) {
  SetColor(color);
  type_ = type;
  stacking_order_ = stacking_order;
  secondary_color_ = secondary_color;
  background_color_ = background_color;
}

void AccessibilityFocusRingLayer::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

  if (SkColorGetA(background_color_) > 0)
    DrawFocusBackground(recorder);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  switch (type_) {
    case FocusRingType::GLOW:
      DrawGlowFocusRing(recorder, flags);
      break;
    case FocusRingType::SOLID:
      DrawSolidFocusRing(recorder, flags);
      break;
    case FocusRingType::DASHED:
      DrawDashedFocusRing(recorder, flags);
      break;
  }
}

void AccessibilityFocusRingLayer::DrawSolidFocusRing(
    ui::PaintRecorder& recorder,
    cc::PaintFlags& flags) {
  CHECK(has_custom_color());

  SkPath path;
  gfx::Vector2d offset = layer()->bounds().OffsetFromOrigin();
  flags.setColor(custom_color());
  flags.setStrokeWidth(kDefaultStrokeWidth);

  path = MakePath(ring_, 0, offset);
  recorder.canvas()->DrawPath(path, flags);

  flags.setColor(secondary_color_);
  path = MakePath(ring_, kDefaultStrokeWidth, offset);
  recorder.canvas()->DrawPath(path, flags);
}

void AccessibilityFocusRingLayer::DrawDashedFocusRing(
    ui::PaintRecorder& recorder,
    cc::PaintFlags& flags) {
  CHECK(has_custom_color());

  SkPath path;
  gfx::Vector2d offset = layer()->bounds().OffsetFromOrigin();

  flags.setColor(custom_color());
  flags.setStrokeWidth(kDefaultStrokeWidth);

  path = MakePath(ring_, 0, offset);
  recorder.canvas()->DrawPath(path, flags);

  SkScalar intervals[] = {kDashLengthDip, kGapLengthDip};
  int intervals_length = 2;
  flags.setPathEffect(cc::PathEffect::MakeDash(intervals, intervals_length, 0));
  flags.setColor(secondary_color_);

  path = MakePath(ring_, kDefaultStrokeWidth, offset);
  recorder.canvas()->DrawPath(path, flags);
}

void AccessibilityFocusRingLayer::DrawGlowFocusRing(ui::PaintRecorder& recorder,
                                                    cc::PaintFlags& flags) {
  CHECK(has_custom_color());
  SkColor base_color = custom_color();

  SkPath path;
  gfx::Vector2d offset = layer()->bounds().OffsetFromOrigin();
  flags.setStrokeWidth(kDefaultStrokeWidth);

  const int w = kGradientWidth;
  for (int i = 0; i < w; ++i) {
    flags.setColor(SkColorSetA(base_color, 255 * (w - i) * (w - i) / (w * w)));
    path = MakePath(ring_, i, offset);
    recorder.canvas()->DrawPath(path, flags);
  }
}

void AccessibilityFocusRingLayer::DrawFocusBackground(
    ui::PaintRecorder& recorder) {
  recorder.canvas()->DrawColor(background_color_);

  gfx::Vector2d offset = layer()->bounds().OffsetFromOrigin();
  SkPath path = MakePath(ring_, 0, offset);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setBlendMode(SkBlendMode::kClear);
  flags.setColor(SkColorSetARGB(0, 0, 0, 0));
  recorder.canvas()->DrawPath(path, flags);
}

}  // namespace ash
