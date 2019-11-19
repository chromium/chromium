// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_NEW_DESK_BUTTON_H_
#define ASH_WM_DESKS_NEW_DESK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "base/macros.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

class DesksBarItemBorder;

// A button view that shows up in the top-right corner of the screen when
// overview mode is on, which is used to create a new virtual desk.
class ASH_EXPORT NewDeskButton
    : public views::LabelButton,
      public OverviewHighlightController::OverviewHighlightableView {
 public:
  explicit NewDeskButton(views::ButtonListener* listener);
  ~NewDeskButton() override = default;

  // Update the button's enable/disable state based on current desks state.
  void UpdateButtonState();

  void OnButtonPressed();

  void SetLabelVisible(bool visible);

  // Gets the minimum size of this view to properly lay out all its contents.
  // |compact| is set to true for compact mode or false for default mode.
  // The view containing this object can use the size returned from this
  // function to decide its own proper size or layout in default or compact
  // mode.
  gfx::Size GetMinSize(bool compact) const;

  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  // LabelButton:
  const char* GetClassName() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  gfx::Rect GetHighlightBoundsInScreen() override;
  gfx::RoundedCornersF GetRoundedCornersRadii() const override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  bool OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  SkColor GetBackgroundColorForTesting() const { return background_color_; }
  bool IsLabelVisibleForTesting() const;

 private:
  void UpdateBorderState();

  // Owned by this View via `View::border_`. This is just a convenient pointer
  // to it.
  DesksBarItemBorder* border_ptr_;

  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(NewDeskButton);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_NEW_DESK_BUTTON_H_
