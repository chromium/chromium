// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_WIDGET_H_
#define ASH_SHELF_HOTSEAT_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ui/views/widget/widget.h"

namespace ash {
class FocusCycler;
class ScrollableShelfView;
class Shelf;
class ShelfView;

// The hotseat widget is part of the shelf and hosts app shortcuts.
class ASH_EXPORT HotseatWidget : public views::Widget,
                                 public HotseatTransitionAnimator::Observer,
                                 public ShelfConfig::Observer {
 public:
  HotseatWidget();
  ~HotseatWidget() override;

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container, Shelf* shelf);

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // ShelfConfig::Observer
  void OnShelfConfigUpdated() override;

  // Whether the overflow menu/bubble is currently being shown.
  bool IsShowingOverflowBubble() const;

  // Whether the widget is in the extended position.
  bool IsExtended() const;

  // Focuses the first or the last app shortcut inside the overflow shelf.
  // Does nothing if the overflow shelf is not currently shown.
  void FocusOverflowShelf(bool last_element);

  // Finds the first or last focusable app shortcut and focuses it.
  void FocusFirstOrLastFocusableChild(bool last);

  // Notifies children of tablet mode state changes.
  void OnTabletModeChanged();

  // Updates the opaque background which functions as the hotseat background.
  void UpdateOpaqueBackground();

  // Sets the focus cycler and adds the hotseat to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);

  bool IsShowingShelfMenu() const;

  ShelfView* GetShelfView();
  const ShelfView* GetShelfView() const;

  // HotseatTransitionAnimator::Observer:
  void OnHotseatTransitionAnimationStarted(HotseatState from_state,
                                           HotseatState to_start) override;
  void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                         HotseatState to_start) override;

  // Returns the bounds of the hotseat background in ScrollableShelfView.
  gfx::Rect GetHotseatBackgroundBounds() const;

  // Gets the background which is used to draw the background of the hotseat.
  ui::Layer* GetOpaqueBackground();

  void SetState(HotseatState state);
  HotseatState state() const { return state_; }

  ScrollableShelfView* scrollable_shelf_view() {
    return scrollable_shelf_view_;
  }

  const ScrollableShelfView* scrollable_shelf_view() const {
    return scrollable_shelf_view_;
  }

  // Whether the widget is in the extended position because of a direct
  // manual user intervention (dragging the hotseat into its extended state).
  // This will return |false| after any visible change in the shelf
  // configuration.
  bool is_manually_extended() { return is_manually_extended_; }

  void set_manually_extended(bool value) { is_manually_extended_ = value; }

 private:
  class DelegateView;

  HotseatState state_ = HotseatState::kShown;

  // View containing the shelf items within an active user session. Owned by
  // the views hierarchy.
  ShelfView* shelf_view_ = nullptr;
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  // The contents view of this widget. Contains |shelf_view_| and the background
  // of the hotseat.
  DelegateView* delegate_view_ = nullptr;

  // Whether the widget is currently extended because the user has manually
  // dragged it. This will be reset with any visible shelf configuration change.
  bool is_manually_extended_ = false;

  DISALLOW_COPY_AND_ASSIGN(HotseatWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_WIDGET_H_
