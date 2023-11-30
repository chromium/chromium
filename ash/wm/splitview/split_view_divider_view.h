// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_

#include "ash/utility/cursor_setter.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

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
  ~SplitViewDividerView() override;

  void DoSpawningAnimation(int spawn_position);
  void SetDividerBarVisible(bool visible);

  // views::View:
  void Layout() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

 private:
  void SwapWindows();

  void OnResizeStatusChanged();

  // TODO(b/314018158): Replace with `LayoutDividerController`.
  raw_ptr<SplitViewController, DanglingUntriaged | ExperimentalAsh>
      split_view_controller_;
  raw_ptr<SplitViewDividerHandlerView, ExperimentalAsh> divider_handler_view_ =
      nullptr;
  raw_ptr<SplitViewDivider, DanglingUntriaged | ExperimentalAsh> divider_;

  // Securely updates the cursor.
  CursorSetter cursor_setter_;
};

}  // namespace ash

#endif
