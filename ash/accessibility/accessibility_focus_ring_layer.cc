// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_focus_ring_layer.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// The number of density-indpendent pixels in the color gradient that fades to
// transparent.
constexpr int kGradientWidth = 6;
constexpr int kDefaultStrokeWidth = 2;
constexpr float kDashLengthDip = 3.f;
constexpr float kGapLengthDip = 5.f;

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
  gfx::Point p0 = ring.points[0] - offset;
  path.moveTo(SkIntToScalar(p0.x()), SkIntToScalar(p0.y()));
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
  int inset = kGradientWidth;
  bounds.Inset(-inset, -inset, -inset, -inset);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  CreateOrUpdateLayer(root_window, "AccessibilityFocusRing", bounds);
}

void AccessibilityFocusRingLayer::SetAppearance(FocusRingType type,
                                                SkColor color,
                                                SkColor secondary_color) {
  SetColor(color);
  type_ = type;
  secondary_color_ = secondary_color;
}

void AccessibilityFocusRingLayer::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

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
  if (!has_custom_color())
    NOTREACHED();

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
  if (!has_custom_color())
    NOTREACHED();

  SkPath path;
  gfx::Vector2d offset = layer()->bounds().OffsetFromOrigin();

  SkScalar intervals[] = {kDashLengthDip, kGapLengthDip};
  int intervals_length = 2;
  flags.setPathEffect(SkDashPathEffect::Make(intervals, intervals_length, 0));

  // To keep the dashes properly lined up, we will draw the outside line first,
  // and cover it with the inner line.
  flags.setColor(secondary_color_);
  flags.setStrokeWidth(2 * kDefaultStrokeWidth);

  path = MakePath(ring_, 0, offset);
  recorder.canvas()->DrawPath(path, flags);

  flags.setColor(custom_color());
  flags.setStrokeWidth(kDefaultStrokeWidth);

  path = MakePath(ring_, 0, offset);
  recorder.canvas()->DrawPath(path, flags);
}

void AccessibilityFocusRingLayer::DrawGlowFocusRing(ui::PaintRecorder& recorder,
                                                    cc::PaintFlags& flags) {
  if (!has_custom_color())
    NOTREACHED();
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

}  // namespace ash
