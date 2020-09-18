// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_NAME_VIEW_H_
#define ASH_WM_DESKS_DESK_NAME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// Defines a special textfield that allows modifying the name of its
// corresponding desk. When it's not focused, it looks like a normal label. It
// can be highlighted and activated by the OverviewHighlightController, and it
// provides an API to elide long desk names.
class ASH_EXPORT DeskNameView
    : public views::Textfield,
      public OverviewHighlightController::OverviewHighlightableView {
 public:
  DeskNameView();
  DeskNameView(const DeskNameView&) = delete;
  DeskNameView& operator=(const DeskNameView&) = delete;
  ~DeskNameView() override;

  // The max number of characters (UTF-16) allowed for desks' names.
  static constexpr size_t kMaxLength = 300;

  // Commits an on-going desk name change (if any) by bluring the focus away
  // from any view on |widget|, where |widget| should be the desks bar widget.
  static void CommitChanges(views::Widget* widget);

  void SetTextAndElideIfNeeded(const base::string16& text);

  // If this view has focus, make the view's border visible and change
  // background to its active color. If it doesn't have focus, hide the view's
  // border and change background to its default color.
  void UpdateViewAppearance();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

 private:
  void UpdateBorderState();

  // Returns the background color for this view based on whether it has focus
  // and if the mouse is entering/exiting the view.
  SkColor GetBackgroundColor() const;

  // Owned by this View via `View::border_`. This is just a convenient pointer
  // to it.
  WmHighlightItemBorder* border_ptr_;

  // Full text without being elided.
  base::string16 full_text_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_NAME_VIEW_H_
