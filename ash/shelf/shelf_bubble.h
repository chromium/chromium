// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUBBLE_H_
#define ASH_SHELF_SHELF_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_background_animator.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shell.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// A base class for all shelf tooltip bubbles.
class ASH_EXPORT ShelfBubble : public views::BubbleDialogDelegateView,
                               public ShelfBackgroundAnimatorObserver {
 public:
  ShelfBubble(views::View* anchor,
              ShelfAlignment alignment,
              SkColor background_co0lor);
  ~ShelfBubble() override;

  // views::BubbleDialogDelegateView
  ax::mojom::Role GetAccessibleWindowRole() override;

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
  // BubbleDialogDelegateView overrides:
  int GetDialogButtons() const override;

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfBackground(SkColor color) override;

  int border_radius_ = 0;

  ShelfBackgroundAnimator background_animator_;

  DISALLOW_COPY_AND_ASSIGN(ShelfBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUBBLE_H_
