// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar.h"

#include "cc/paint/paint_flags.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

FakeScrollbar::FakeScrollbar() = default;
FakeScrollbar::~FakeScrollbar() = default;

bool FakeScrollbar::IsSame(const Scrollbar& other) const {
  return this == &other;
}

ScrollbarOrientation FakeScrollbar::Orientation() const {
  return orientation_;
}

bool FakeScrollbar::IsLeftSideVerticalScrollbar() const {
  return is_left_side_vertical_scrollbar_;
}

bool FakeScrollbar::IsSolidColor() const {
  return is_solid_color_;
}

bool FakeScrollbar::IsOverlay() const { return is_overlay_; }

bool FakeScrollbar::HasThumb() const { return has_thumb_; }

gfx::Rect FakeScrollbar::ThumbRect() const {
  // The location of ThumbRect doesn't matter in cc.
  return gfx::Rect(thumb_size_);
}

gfx::Rect FakeScrollbar::BackButtonRect() const {
  return back_button_rect_;
}

gfx::Rect FakeScrollbar::ForwardButtonRect() const {
  return forward_button_rect_;
}

bool FakeScrollbar::JumpOnTrackClick() const {
  return false;
}

bool FakeScrollbar::SupportsDragSnapBack() const {
  return false;
}

gfx::Rect FakeScrollbar::TrackRect() const {
  return track_rect_;
}

float FakeScrollbar::Opacity() const {
  return thumb_opacity_;
}

bool FakeScrollbar::NeedsRepaintPart(ScrollbarPart part) const {
  if (part == ScrollbarPart::THUMB)
    return needs_repaint_thumb_;
  return needs_repaint_track_;
}

bool FakeScrollbar::HasTickmarks() const {
  return has_tickmarks_;
}

void FakeScrollbar::PaintPart(PaintCanvas* canvas,
                              ScrollbarPart part,
                              const gfx::Rect& rect) {
  if (!should_paint_)
    return;

  // Fill the scrollbar with a different color each time.
  fill_color_++;
  PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setColor(paint_fill_color());
  flags.setStyle(PaintFlags::kFill_Style);
  canvas->drawRect(RectToSkRect(rect), flags);
}

bool FakeScrollbar::UsesNinePatchThumbResource() const {
  return uses_nine_patch_thumb_resource_;
}

gfx::Size FakeScrollbar::NinePatchThumbCanvasSize() const {
  return uses_nine_patch_thumb_resource_ ? gfx::Size(5, 5) : gfx::Size();
}

gfx::Rect FakeScrollbar::NinePatchThumbAperture() const {
  return uses_nine_patch_thumb_resource_ ? gfx::Rect(0, 0, 5, 5) : gfx::Rect();
}

}  // namespace cc
