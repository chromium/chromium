// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_MINI_VIEW_H_
#define ASH_WM_DESKS_DESK_MINI_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_observer.h"

namespace ash {

class CloseDeskButton;
class DeskNameView;
class DeskPreviewView;
class DesksBarView;

// A view that acts as a mini representation (a.k.a. desk thumbnail) of a
// virtual desk in the desk bar view when overview mode is active. This view
// shows a preview of the contents of the associated desk, its title, and
// supports desk activation and removal.
class ASH_EXPORT DeskMiniView
    : public views::View,
      public views::ButtonListener,
      public Desk::Observer,
      public OverviewHighlightController::OverviewHighlightableView,
      public views::TextfieldController,
      public views::ViewObserver {
 public:
  DeskMiniView(DesksBarView* owner_bar, aura::Window* root_window, Desk* desk);
  ~DeskMiniView() override;

  aura::Window* root_window() { return root_window_; }

  Desk* desk() { return desk_; }

  DeskNameView* desk_name_view() { return desk_name_view_; }

  const CloseDeskButton* close_desk_button() const {
    return close_desk_button_;
  }

  // Returns the associated desk's container window on the display this
  // mini_view resides on.
  aura::Window* GetDeskContainer() const;

  // Returns true if the desk's name is being modified (i.e. the DeskNameView
  // has the focus).
  bool IsDeskNameBeingModified() const;

  // Updates the visibility state of the close button depending on whether this
  // view is mouse hovered, or if switch access is enabled.
  void UpdateCloseButtonVisibility();

  // Gesture tapping may affect the visibility of the close button. There's only
  // one mini_view that shows the close button on long press at any time.
  // This is useful for touch-only UIs.
  void OnWidgetGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Updates the border color of the DeskPreviewView based on the activation
  // state of the corresponding desk.
  void UpdateBorderColor();

  // views::Button:
  const char* GetClassName() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Desk::Observer:
  void OnContentChanged() override;
  void OnDeskDestroyed(const Desk* desk) override;
  void OnDeskNameChanged(const base::string16& new_name) override;

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  bool IsPointOnMiniView(const gfx::Point& screen_location) const;

  // Gets the minimum width of this view to properly lay out all its contents in
  // default layout.
  // The view containing this object can use the width returned from this
  // function to decide its own proper size or layout.
  int GetMinWidthForDefaultLayout() const;

  bool IsDeskNameViewVisibleForTesting() const;
  const DeskPreviewView* GetDeskPreviewForTesting() const {
    return desk_preview_;
  }

 private:
  void OnCloseButtonPressed();

  void OnDeskPreviewPressed();

  // Layout |desk_name_view_| given the current bounds of the desk preview.
  void LayoutDeskNameView(const gfx::Rect& preview_bounds);

  DesksBarView* const owner_bar_;

  // The root window on which this mini_view is created.
  aura::Window* root_window_;

  // The associated desk. Can be null when the desk is deleted before this
  // mini_view completes its removal animation. See comment above
  // OnDeskRemoved().
  Desk* desk_;  // Not owned.

  // The view that shows a preview of the desk contents.
  DeskPreviewView* desk_preview_;

  // The editable desk name.
  DeskNameView* desk_name_view_;

  // The close button that shows on hover.
  CloseDeskButton* close_desk_button_;

  // We force showing the close button when the mini_view is long pressed or
  // tapped using touch gestures.
  bool force_show_close_button_ = false;

  // When the DeskNameView is focused, we select all its text. However, if it is
  // focused via a mouse press event, on mouse release will clear the selection.
  // Therefore, we defer selecting all text until we receive that mouse release.
  bool defer_select_all_ = false;

  bool is_desk_name_being_modified_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeskMiniView);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_MINI_VIEW_H_
