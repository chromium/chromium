// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_layer.h"

#include "ui/aura/window.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"

namespace ui {
class Compositor;
}

namespace ash {

namespace {

const int kShadowRadius = 10;
const int kShadowAlpha = 90;
const SkColor kShadowColor = SkColorSetRGB(77, 144, 254);

}  // namespace

FocusRingLayer::FocusRingLayer(AccessibilityLayerDelegate* delegate)
    : AccessibilityLayer(delegate) {}

FocusRingLayer::~FocusRingLayer() = default;

void FocusRingLayer::SetColor(SkColor color) {
  custom_color_ = color;
}

void FocusRingLayer::ResetColor() {
  custom_color_.reset();
}

int FocusRingLayer::GetInset() const {
  return kShadowRadius + 2;
}

void FocusRingLayer::OnPaintLayer(const ui::PaintContext& context) {
  if (!root_window_ || layer_rect_.IsEmpty())
    return;

  ui::PaintRecorder recorder(context, layer_->size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(custom_color_ ? *custom_color_ : kShadowColor);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2);

  gfx::Rect bounds = layer_rect_ - layer_->bounds().OffsetFromOrigin();
  int r = kShadowRadius;
  for (int i = 0; i < r; i++) {
    // Fade out alpha quadratically.
    flags.setAlphaf((kShadowAlpha * (r - i) * (r - i)) / (r * r) / 255.0f);
    gfx::Rect outsetRect = bounds;
    outsetRect.Inset(-i);
    recorder.canvas()->DrawRect(outsetRect, flags);
  }
}

}  // namespace ash
