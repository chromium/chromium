// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_

#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class SplitViewController;
class SplitViewDivider;
class SplitViewDividerHandlerView;

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
  void OnThemeChanged() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

 private:
  void OnResizeStatusChanged();

  SplitViewController* split_view_controller_;
  SplitViewDividerHandlerView* divider_handler_view_ = nullptr;
  SplitViewDivider* divider_;
};

}  // namespace ash

#endif
