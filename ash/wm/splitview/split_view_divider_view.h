// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_

#include "ash/style/icon_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class SplitViewController;
class SplitViewDivider;
class SplitViewDividerHandlerView;
class SnapGroupExpandedMenuView;

// A view that acts as the contents view of the split view divider widget.
class SplitViewDividerView : public views::View,
                             public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(SplitViewDividerView);

  explicit SplitViewDividerView(SplitViewController* controller,
                                SplitViewDivider* divider);
  SplitViewDividerView(const SplitViewDividerView&) = delete;
  SplitViewDividerView& operator=(const SplitViewDividerView&) = delete;
  ~SplitViewDividerView() override = default;

  void DoSpawningAnimation(int spawn_position);
  void SetDividerBarVisible(bool visible);

  // views::View:
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  IconButton* kebab_button_for_testing() const { return kebab_button_; }
  views::Widget* snap_group_expanded_menu_widget_for_testing() const {
    return snap_group_expanded_menu_widget_.get();
  }
  SnapGroupExpandedMenuView* snap_group_expanded_menu_view_for_testing() const {
    return snap_group_expanded_menu_view_;
  }

 private:
  void OnResizeStatusChanged();

  // Called when the `kebab_button_` is pressed which toggles between showing or
  // hiding the `snap_group_expanded_menu_widget_`.
  void OnKebabButtonPressed();

  // Called to update the bounds of the `snap_group_expanded_menu_widget_`.
  void MaybeUpdateExpandedMenuWidgetBounds();

  raw_ptr<SplitViewController, ExperimentalAsh> split_view_controller_;
  raw_ptr<SplitViewDividerHandlerView, ExperimentalAsh> divider_handler_view_ =
      nullptr;
  raw_ptr<SplitViewDivider, ExperimentalAsh> divider_;

  // A vertical 3-dot button that shows on the split view divider when
  // `ShouldAutomaticallyGroupOnWindowsSnappedInClamshell()` is true.
  raw_ptr<IconButton, ExperimentalAsh> kebab_button_;

  // The snap group expanded menu widget and its contents view.
  views::UniqueWidgetPtr snap_group_expanded_menu_widget_;
  raw_ptr<SnapGroupExpandedMenuView, ExperimentalAsh>
      snap_group_expanded_menu_view_ = nullptr;

  // True if the `snap_group_expanded_menu_widget_` should show
  // `OnKebabButtonPressed()` and false otherwise. The value will be updated on
  // the `kebab_button_` is clicked.
  bool should_show_expanded_menu_ = false;
};

}  // namespace ash

#endif
