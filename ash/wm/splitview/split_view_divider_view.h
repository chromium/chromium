// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class DividerHandlerView;
class SplitViewDivider;

// A view that acts as the content view within a split view divider widget.
// It hosts one child view: a handler view. Its responsibility is to update the
// bounds and visibility of its child views in response to located events.
//          | |
//          | |
//          |||<-----handler_view_
//          |||
//          | |
class SplitViewDividerView : public views::AccessiblePaneView,
                             public views::ViewTargeterDelegate {
  METADATA_HEADER(SplitViewDividerView, views::AccessiblePaneView)

 public:
  explicit SplitViewDividerView(SplitViewDivider* divider);
  SplitViewDividerView(const SplitViewDividerView&) = delete;
  SplitViewDividerView& operator=(const SplitViewDividerView&) = delete;
  ~SplitViewDividerView() override;

  void SetHandlerBarVisible(bool visible);

  // Called explicitly by SplitViewDivider when the divider widget is closing.
  void OnDividerClosing();

  // views::View:
  void Layout(PassKey) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;
  void OnFocus() override;

  ASH_EXPORT gfx::Rect GetHandlerViewBoundsInScreenForTesting() const;

  DividerHandlerView* handler_view_for_testing() { return handler_view_; }

 private:
  friend class SplitViewDivider;

  void SwapWindows();

  void StartResizing(gfx::Point location);

  // Safely ends resizing, preventing use after destruction. If
  // `swap_windows` is true, swaps the windows after resizing.
  void EndResizing(gfx::Point location, bool swap_windows);

  // Resizes the windows and divider on a key event.
  void ResizeOnKeyEvent(bool left_or_top, bool horizontal);

  // Refreshes the divider handler's bounds and rounded corners in response to
  // changes in the divider's hover state or display properties.
  void RefreshDividerHandler();

  // The location of the initial mouse event in screen coordinates.
  gfx::Point initial_mouse_event_location_;

  // True if the mouse has been pressed down and moved (dragged) so we can start
  // a resize.
  bool mouse_move_started_ = false;

  raw_ptr<SplitViewDivider> divider_;

  raw_ptr<DividerHandlerView> handler_view_ = nullptr;

  base::WeakPtrFactory<SplitViewDividerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_
