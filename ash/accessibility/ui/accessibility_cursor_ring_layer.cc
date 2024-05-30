// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_cursor_ring_layer.h"

#include "ash/public/cpp/shell_window_ids.h"
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
  bounds.Inset(-inset);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  // Root could be null if window tree host is being updated. See
  // http://b/326074244 for more details.
  if (!root_window) {
    return;
  }

  aura::Window* container = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityBubbleContainer);
  wm::ConvertRectFromScreen(container, &bounds);
  CreateOrUpdateLayer(container, "AccessibilityCursorRing", bounds,
                      /*stack_at_top=*/true);
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
  r.Inset(kLayerMargin);
  const int w = kGradientWidth;
  for (int i = 0; i < w; ++i) {
    flags.setColor(SkColorSetARGB(255 * i * i / (w * w), red_, green_, blue_));
    SkPath path;
    path.addOval(SkRect::MakeXYWH(r.x(), r.y(), r.width(), r.height()));
    r.Inset(1);
    recorder.canvas()->DrawPath(path, flags);
  }
}

}  // namespace ash
