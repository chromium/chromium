// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

class IconButton;
class SplitViewDivider;
class SplitViewDividerHandlerView;

// A view that acts as the contents view of the split view divider widget.
class SplitViewDividerView : public views::View,
                             public views::ViewTargeterDelegate {
  METADATA_HEADER(SplitViewDividerView, views::View)

 public:
  explicit SplitViewDividerView(SplitViewDivider* divider);
  SplitViewDividerView(const SplitViewDividerView&) = delete;
  SplitViewDividerView& operator=(const SplitViewDividerView&) = delete;
  ~SplitViewDividerView() override;

  void DoSpawningAnimation(int spawn_position);
  void SetDividerBarVisible(bool visible);

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

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  IconButton* feedback_button_for_testing() const { return feedback_button_; }

 private:
  void SwapWindows();

  void OnResizeStatusChanged();

  void StartResizing(gfx::Point location);

  // Safely ends resizing, preventing use after destruction. If
  // `swap_windows` is true, swaps the windows after resizing.
  void EndResizing(gfx::Point location, bool swap_windows);

  // Initializes, refreshes bounds, or updates visibility for the
  // `feedback_button_` on the divider.
  void RefreshFeedbackButton(bool visible);

  // Refreshes the bounds of the `feedback_button_`.
  void RefreshFeedbackButtonBounds();

  // Triggered when the feedback button is pressed to open feedback form.
  void OnFeedbackButtonPressed();

  // The location of the initial mouse event in screen coordinates.
  gfx::Point initial_mouse_event_location_;

  // True if the mouse has been pressed down and moved (dragged) so we can start
  // a resize.
  bool mouse_move_started_ = false;

  raw_ptr<SplitViewDividerHandlerView> divider_handler_view_ = nullptr;
  raw_ptr<SplitViewDivider> divider_;

  raw_ptr<IconButton> feedback_button_ = nullptr;

  base::WeakPtrFactory<SplitViewDividerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif
