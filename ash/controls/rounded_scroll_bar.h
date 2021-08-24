// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTROLS_ROUNDED_SCROLL_BAR_H_
#define ASH_CONTROLS_ROUNDED_SCROLL_BAR_H_

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace ash {

// A scrollbar similar to views::OverlayScrollBar but with ash styling.
// - Shows the thumb on scroll and on hover
// - Fades out the thumb after a delay
// - Draws the thumb with rounded ends
class RoundedScrollBar : public views::ScrollBar {
 public:
  explicit RoundedScrollBar(bool horizontal);
  RoundedScrollBar(const RoundedScrollBar&) = delete;
  RoundedScrollBar& operator=(const RoundedScrollBar&) = delete;
  ~RoundedScrollBar() override;

  // views::ScrollBar:
  gfx::Rect GetTrackBounds() const override;
  bool OverlapsContent() const override;
  int GetThickness() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void ScrollToPosition(int position) override;
  void ObserveScrollEvent(const ui::ScrollEvent& event) override;

 private:
  class Thumb;

  void ShowScrollbar();
  void HideScrollBar();

  // When the mouse is hovering over the scrollbar, the scrollbar should always
  // be displayed.
  bool mouse_over_scrollbar_ = false;

  // Timer that will start the scrollbar's hiding animation when it reaches 0.
  base::RetainingOneShotTimer hide_scrollbar_timer_;
};

}  // namespace ash

#endif  // ASH_CONTROLS_ROUNDED_SCROLL_BAR_H_
