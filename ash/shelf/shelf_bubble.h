// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUBBLE_H_
#define ASH_SHELF_SHELF_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_background_animator.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shell.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// A base class for all shelf tooltip bubbles.
class ASH_EXPORT ShelfBubble : public views::BubbleDialogDelegateView,
                               public ShelfBackgroundAnimatorObserver {
  METADATA_HEADER(ShelfBubble, views::BubbleDialogDelegateView)

 public:
  ShelfBubble(views::View* anchor,
              ShelfAlignment alignment,
              bool for_tooltip,
              std::optional<views::BubbleBorder::Arrow> arrow_position);

  ShelfBubble(const ShelfBubble&) = delete;
  ShelfBubble& operator=(const ShelfBubble&) = delete;

  ~ShelfBubble() override;

  // Returns true if we should close when we get a press down event within our
  // bounds.
  virtual bool ShouldCloseOnPressDown() = 0;

  // Returns true if we should disappear when the mouse leaves the anchor's
  // bounds.
  virtual bool ShouldCloseOnMouseExit() = 0;

 protected:
  void set_border_radius(int radius) { border_radius_ = radius; }

  // Performs the actual bubble creation.
  void CreateBubble();

 private:
  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfBackground(SkColor color) override;

  // views::BubbleDialogDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  int border_radius_ = 0;

  const bool for_tooltip_;

  ShelfBackgroundAnimator background_animator_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUBBLE_H_
