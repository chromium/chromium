// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCROLLBAR_H_
#define CC_TEST_FAKE_SCROLLBAR_H_

#include "base/compiler_specific.h"
#include "cc/input/scrollbar.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class FakeScrollbar : public Scrollbar {
 public:
  FakeScrollbar();
  FakeScrollbar(bool paint, bool has_thumb, bool is_overlay);
  FakeScrollbar(bool paint,
                bool has_thumb,
                ScrollbarOrientation orientation,
                bool is_left_side_vertical_scrollbar,
                bool is_overlay);
  FakeScrollbar(const FakeScrollbar&) = delete;

  FakeScrollbar& operator=(const FakeScrollbar&) = delete;

  // Scrollbar implementation.
  ScrollbarOrientation Orientation() const override;
  bool IsLeftSideVerticalScrollbar() const override;
  bool IsSolidColor() const override;
  bool IsOverlay() const override;
  bool HasThumb() const override;
  gfx::Rect ThumbRect() const override;
  gfx::Rect BackButtonRect() const override;
  gfx::Rect ForwardButtonRect() const override;
  bool SupportsDragSnapBack() const override;
  gfx::Rect TrackRect() const override;
  float ThumbOpacity() const override;
  bool NeedsRepaintPart(ScrollbarPart part) const override;
  bool HasTickmarks() const override;
  void PaintPart(PaintCanvas* canvas,
                 ScrollbarPart part,
                 const gfx::Rect& rect) override;
  bool UsesNinePatchThumbResource() const override;
  gfx::Size NinePatchThumbCanvasSize() const override;
  gfx::Rect NinePatchThumbAperture() const override;

  void set_track_rect(const gfx::Rect& track_rect) { track_rect_ = track_rect; }
  void set_thumb_size(const gfx::Size& thumb_size) { thumb_size_ = thumb_size; }
  void set_has_thumb(bool has_thumb) { has_thumb_ = has_thumb; }
  SkColor paint_fill_color() const { return SK_ColorBLACK | fill_color_; }

  void set_thumb_opacity(float opacity) { thumb_opacity_ = opacity; }
  void set_needs_repaint_thumb(bool needs_repaint) {
    needs_repaint_thumb_ = needs_repaint;
  }
  void set_needs_repaint_track(bool needs_repaint) {
    needs_repaint_track_ = needs_repaint;
  }
  void set_has_tickmarks(bool has_tickmarks) { has_tickmarks_ = has_tickmarks; }

 protected:
  ~FakeScrollbar() override;

 private:
  bool paint_;
  bool has_thumb_;
  ScrollbarOrientation orientation_;
  bool is_left_side_vertical_scrollbar_;
  bool is_overlay_;
  gfx::Size thumb_size_;
  float thumb_opacity_;
  bool needs_repaint_thumb_;
  bool needs_repaint_track_;
  bool has_tickmarks_;
  gfx::Rect track_rect_;
  gfx::Rect back_button_rect_;
  gfx::Rect forward_button_rect_;
  SkColor fill_color_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCROLLBAR_H_
