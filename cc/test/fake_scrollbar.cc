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

bool FakeScrollbar::IsRunningWebTest() const {
  return true;
}

bool FakeScrollbar::IsFluentOverlayScrollbarMinimalMode() const {
  return false;
}

gfx::Rect FakeScrollbar::ShrinkMainThreadedMinimalModeThumbRect(
    gfx::Rect& rect) const {
  return rect;
}

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

bool FakeScrollbar::ThumbNeedsRepaint() const {
  return thumb_needs_repaint_;
}

void FakeScrollbar::ClearThumbNeedsRepaint() {
  set_thumb_needs_repaint(false);
}

bool FakeScrollbar::TrackAndButtonsNeedRepaint() const {
  return track_and_buttons_need_repaint_;
}

bool FakeScrollbar::NeedsUpdateDisplay() const {
  return needs_update_display_;
}

void FakeScrollbar::ClearNeedsUpdateDisplay() {
  needs_update_display_ = false;
}

bool FakeScrollbar::HasTickmarks() const {
  return has_tickmarks_;
}

void FakeScrollbar::PaintThumb(PaintCanvas& canvas, const gfx::Rect& rect) {
  Paint(canvas, rect);
}

void FakeScrollbar::PaintTrackAndButtons(PaintCanvas& canvas,
                                         const gfx::Rect& rect) {
  Paint(canvas, rect);
}

void FakeScrollbar::Paint(PaintCanvas& canvas, const gfx::Rect& rect) {
  if (!should_paint_)
    return;

  // Fill the scrollbar with a different color each time.
  fill_color_++;
  PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setColor(paint_fill_color());
  flags.setStyle(PaintFlags::kFill_Style);
  canvas.drawRect(RectToSkRect(rect), flags);
}

SkColor4f FakeScrollbar::ThumbColor() const {
  return thumb_color_;
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

bool FakeScrollbar::UsesSolidColorThumb() const {
  return uses_solid_color_thumb_;
}

gfx::Insets FakeScrollbar::SolidColorThumbInsets() const {
  return gfx::Insets();
}

bool FakeScrollbar::UsesNinePatchTrackAndButtonsResource() const {
  return uses_nine_patch_track_and_buttons_resource_;
}

gfx::Size FakeScrollbar::NinePatchTrackAndButtonsCanvasSize() const {
  return uses_nine_patch_track_and_buttons_resource_ ? gfx::Size(5, 5)
                                                     : gfx::Size();
}

gfx::Rect FakeScrollbar::NinePatchTrackAndButtonsAperture() const {
  return uses_nine_patch_track_and_buttons_resource_ ? gfx::Rect(0, 0, 5, 5)
                                                     : gfx::Rect();
}

bool FakeScrollbar::IsOpaque() const {
  return !is_overlay_ && is_opaque_;
}

}  // namespace cc
