// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_cursor_ring_layer.h"

#include "ash/shell.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The number of pixels in the color gradient that fades to transparent.
const int kGradientWidth = 8;

// The radius of the ring in pixels.
const int kCursorRingRadius = 24;

// Extra margin to add to the layer in pixels.
const int kLayerMargin = 8;

}  // namespace

AccessibilityCursorRingLayer::AccessibilityCursorRingLayer(
    AccessibilityLayerDelegate* delegate,
    int red,
    int green,
    int blue)
    : FocusRingLayer(delegate), red_(red), green_(green), blue_(blue) {}

AccessibilityCursorRingLayer::~AccessibilityCursorRingLayer() = default;

void AccessibilityCursorRingLayer::Set(const gfx::Point& location) {
  location_ = location;

  gfx::Rect bounds = gfx::Rect(location.x(), location.y(), 0, 0);
  int inset = kGradientWidth + kCursorRingRadius + kLayerMargin;
  bounds.Inset(-inset, -inset, -inset, -inset);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  ::wm::ConvertRectFromScreen(root_window, &bounds);
  CreateOrUpdateLayer(root_window, "AccessibilityCursorRing", bounds);
}

void AccessibilityCursorRingLayer::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2);

  gfx::Rect r = layer()->bounds();
  r.Offset(-r.OffsetFromOrigin());
  r.Inset(kLayerMargin, kLayerMargin, kLayerMargin, kLayerMargin);
  const int w = kGradientWidth;
  for (int i = 0; i < w; ++i) {
    flags.setColor(SkColorSetARGB(255 * i * i / (w * w), red_, green_, blue_));
    SkPath path;
    path.addOval(SkRect::MakeXYWH(r.x(), r.y(), r.width(), r.height()));
    r.Inset(1, 1, 1, 1);
    recorder.canvas()->DrawPath(path, flags);
  }
}

}  // namespace ash
