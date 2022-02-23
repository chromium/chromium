// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_ZERO_STATE_BUTTON_H_
#define ASH_WM_DESKS_ZERO_STATE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class DesksBarView;
class WmHighlightItemBorder;

// The base class of ZeroStateDefaultDeskButton, ZeroStateIconButton and
// the InnerExpandedDesksBarButton of ExpandedDesksBarButton.
class ASH_EXPORT DeskButtonBase : public views::LabelButton,
                                  public OverviewHighlightableView {
 public:
  METADATA_HEADER(DeskButtonBase);

  // This LabelButton will include either text or image inside. Set the text
  // of the button to `text` only if `set_text` is true, otherwise, the given
  // `text` will only be used for the tooltip, accessible name etc of the
  // button. If text of the button is empty, an image will be assigned to the
  // button instead.
  DeskButtonBase(const std::u16string& text, bool set_text);
  DeskButtonBase(const std::u16string& text,
                 bool set_text,
                 int border_corder_radius,
                 int corner_radius);
  ~DeskButtonBase() override = default;

  WmHighlightItemBorder* border_ptr() { return border_ptr_; }

  // LabelButton:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  virtual void SetButtonState(bool enabled) {}

  // Updates the label's text of the button. E.g, ZeroStateDefaultDeskButton
  // showing the desk's name, which should be updated on desk name changes.
  virtual void UpdateLabelText() {}

  // Sets `should_paint_background_` and repaints the button so that the button
  // may or may not have the background.
  void SetShouldPaintBackground(bool should_paint_background);

 protected:
  virtual void OnButtonPressed() = 0;

  virtual void UpdateBorderState();

  SkColor background_color_;

  // If true, paints a background of the button with `background_color_`. The
  // button is painted with the background by default, exception like
  // ZeroStateIconButton only wants to be painted when the mouse hovers.
  bool should_paint_background_ = true;

  // Paints the background within the button's bounds by default. But if true,
  // paints the contents' bounds of the button only. For example,
  // InnerExpandedDesksBarButton needs to be kept as the same size of the desk
  // preview, which has a gap between the view's contents and the border.
  bool paint_contents_only_ = false;

 private:
  friend class DesksTestApi;

  // Owned by this View via `View::border_`. This is just a convenient pointer
  // to it.
  WmHighlightItemBorder* border_ptr_;

  const int corner_radius_;
};

// A button in zero state bar showing "Desk 1". Zero state is the state of the
// desks bar when there's only a single desk available, in which case the bar is
// shown in a minimized state. Clicking the button will switch to the expanded
// desks bar and focus on the single desk's name view. The expanded bar will
// include the single desk and the ExpandedDesksBarButton.
class ASH_EXPORT ZeroStateDefaultDeskButton : public DeskButtonBase {
 public:
  METADATA_HEADER(ZeroStateDefaultDeskButton);

  explicit ZeroStateDefaultDeskButton(DesksBarView* bar_view);
  ZeroStateDefaultDeskButton(const ZeroStateDefaultDeskButton&) = delete;
  ZeroStateDefaultDeskButton& operator=(const ZeroStateDefaultDeskButton&) =
      delete;
  ~ZeroStateDefaultDeskButton() override = default;

  // DeskButtonBase:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnButtonPressed() override;
  void UpdateLabelText() override;

 private:
  DesksBarView* bar_view_;
};

// A button in the zero state bar with an icon. Zero state is the state of the
// desks bar when there's only a single desk available, in which case the bar is
// shown in a minimized state.
class ASH_EXPORT ZeroStateIconButton : public DeskButtonBase {
 public:
  METADATA_HEADER(ZeroStateIconButton);

  ZeroStateIconButton(const gfx::VectorIcon* button_icon,
                      const std::u16string& text,
                      base::RepeatingClosure callback);
  ZeroStateIconButton(const ZeroStateIconButton&) = delete;
  ZeroStateIconButton& operator=(const ZeroStateIconButton&) = delete;
  ~ZeroStateIconButton() override;

  // DeskButtonBase:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnButtonPressed() override;

  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  const gfx::VectorIcon* const button_icon_;

  // Defines the button behavior and is called in OnButtonPressed.
  base::RepeatingClosure button_callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_ZERO_STATE_BUTTON_H_
