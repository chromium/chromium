// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_ZERO_STATE_BUTTON_H_
#define ASH_WM_DESKS_ZERO_STATE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

class DesksBarView;
class WmHighlightItemBorder;

// The base class of ZeroStateDefaultDeskButton, ZeroStateNewDeskButton and
// the InnerNewDeskButton of ExpandedStateNewDeskButton.
class ASH_EXPORT DeskButtonBase
    : public views::LabelButton,
      public OverviewHighlightController::OverviewHighlightableView {
 public:
  DeskButtonBase(const base::string16& text,
                 int border_corder_radius,
                 int corner_radius);
  ~DeskButtonBase() override = default;

  // LabelButton:
  const char* GetClassName() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  void OnThemeChanged() override;

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  virtual void UpdateButtonState() {}

  // Updates the label's text of the button. E.g, ZeroStateDefaultDeskButton
  // showing the desk's name, which should be updated on desk name changes.
  virtual void UpdateLabelText() {}

  SkColor GetBackgroundColorForTesting() const { return background_color_; }

 protected:
  virtual void OnButtonPressed() = 0;

  SkColor background_color_;

  // If true, paints the button with the background of |background_color_|. The
  // button is painted with the background by default, exception like
  // ZeroStateNewDeskButton only wants to be painted when the mouse hovers.
  bool highlight_on_hover_ = true;

  // Paints the background within the button's bounds by default. But if true,
  // paints the contents' bounds of the button only. For example,
  // InnerNewDeskButton needs to be kept as the same size of the desk preview,
  // which has a gap between the view's contents and the border.
  bool paint_contents_only_ = false;

 private:
  void UpdateBorderState();

  // Owned by this View via `View::border_`. This is just a convenient pointer
  // to it.
  WmHighlightItemBorder* border_ptr_;

  const int corner_radius_;
};

// A button in zero state bar showing "Desk 1". Zero state is the state of the
// desks bar when there's only a single desk available, in which case the bar is
// shown in a minimized state. Clicking the button will switch to the expanded
// desks bar and focus on the single desk's name view. The expanded bar will
// include the single desk and the ExpandedStateNewDeskButton.
class ASH_EXPORT ZeroStateDefaultDeskButton : public DeskButtonBase {
 public:
  ZeroStateDefaultDeskButton(DesksBarView* bar_view);
  ZeroStateDefaultDeskButton(const ZeroStateDefaultDeskButton&) = delete;
  ZeroStateDefaultDeskButton& operator=(const ZeroStateDefaultDeskButton&) =
      delete;
  ~ZeroStateDefaultDeskButton() override = default;

  // DeskButtonBase:
  const char* GetClassName() const override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnButtonPressed() override;
  void UpdateLabelText() override;

 private:
  DesksBarView* bar_view_;
};

// A button in zero state bar with a plus icon. Zero state is the state of the
// desks bar when there's only a single desk available, in which case the bar is
// shown in a minimized state. Clicking the button will create a new desk,
// switch to the expanded desks bar and focus on the newly-created desks's name
// view. The expanded bar will include two desks and the
// ExpandedStateNewDeskButton.
class ASH_EXPORT ZeroStateNewDeskButton : public DeskButtonBase {
 public:
  ZeroStateNewDeskButton();
  ZeroStateNewDeskButton(const ZeroStateNewDeskButton&) = delete;
  ZeroStateNewDeskButton& operator=(const ZeroStateNewDeskButton&) = delete;
  ~ZeroStateNewDeskButton() override = default;

  // DeskButtonBase:
  const char* GetClassName() const override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnButtonPressed() override;

  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_ZERO_STATE_BUTTON_H_
