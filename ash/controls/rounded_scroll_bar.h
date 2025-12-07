// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTROLS_ROUNDED_SCROLL_BAR_H_
#define ASH_CONTROLS_ROUNDED_SCROLL_BAR_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace views {
class BaseScrollBarThumb;
}  // namespace views

namespace ash {

// A scrollbar similar to views::OverlayScrollBar but with ash styling.
// - Shows the thumb on scroll and on hover
// - Fades out the thumb after a delay
// - Draws the thumb with rounded ends
// - Becomes brighter when the cursor is over the thumb
class ASH_EXPORT RoundedScrollBar : public views::ScrollBar {
  METADATA_HEADER(RoundedScrollBar, views::ScrollBar)

 public:
  explicit RoundedScrollBar(Orientation orientation);
  RoundedScrollBar(const RoundedScrollBar&) = delete;
  RoundedScrollBar& operator=(const RoundedScrollBar&) = delete;
  ~RoundedScrollBar() override;

  // Sets the insets for the scroll track. Useful if the scroll view is adjacent
  // to the edge of the screen or the edge of a widget.
  void SetInsets(const gfx::Insets& insets);

  // Sets whether a drag that starts on the scroll thumb and then moves far
  // outside the thumb should "snap back" to the original scroll position.
  void SetSnapBackOnDragOutside(bool snap);

  // Sets whether the scroll bar should show itself when the scroll thumb
  // bounds are changed (i.e. when the scroll position changes or the content
  // size changes).
  void SetShowOnThumbBoundsChanged(bool show);

  // views::ScrollBar:
  gfx::Rect GetTrackBounds() const override;
  bool OverlapsContent() const override;
  int GetThickness() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void ScrollToPosition(int position) override;
  void ObserveScrollEvent(const ui::ScrollEvent& event) override;

  // Controls the visibility of the scroll thumb.
  // By default, the thumb is hidden when there's no interaction with the scroll
  // bar (`always_show_thumb_` is false by default). Set `always_show_thumb_` to
  // `true` to keep the thumb visible at all times.
  void SetAlwaysShowThumb(bool always_show_thumb);

  views::BaseScrollBarThumb* GetThumbForTest() const;

 private:
  class Thumb;

  // Shows the scrollbar and/or updates its opacity.
  void ShowScrollbar();

  // Hides the scrollbar if the mouse is outside the scroll track.
  void HideScrollBar();

  // Called when the thumb hover/pressed state changed.
  void OnThumbStateChanged(views::Button::ButtonState old_state);

  // Called when the thumb bounds (position or size) changed.
  void OnThumbBoundsChanged();

  // Equivalent to GetThumb() but typed as the inner class `Thumb`.
  const raw_ptr<Thumb> thumb_;

  // Insets for the scroll track.
  gfx::Insets insets_;

  // Timer that will start the scrollbar's hiding animation when it reaches 0.
  base::RetainingOneShotTimer hide_scrollbar_timer_;

  // Whether to temporarily show the scroll bar when the thumb bounds change.
  bool show_on_thumb_bounds_changed_ = false;

  // Whether to always show the scroll thumb.
  bool always_show_thumb_ = false;
};

}  // namespace ash

#endif  // ASH_CONTROLS_ROUNDED_SCROLL_BAR_H_
