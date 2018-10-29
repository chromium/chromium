// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray_drag_controller.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {
class Shelf;
class TrayBackground;
class TrayContainer;
class TrayEventFilter;

// Base class for some children of StatusAreaWidget. This class handles setting
// and animating the background when the Launcher is shown/hidden. It also
// inherits from ActionableView so that the tray items can override
// PerformAction when clicked on.
class ASH_EXPORT TrayBackgroundView : public ActionableView,
                                      public ui::ImplicitAnimationObserver,
                                      public ShelfBackgroundAnimatorObserver,
                                      public TrayBubbleView::Delegate,
                                      public VirtualKeyboardModel::Observer {
 public:
  static const char kViewClassName[];

  explicit TrayBackgroundView(Shelf* shelf);
  ~TrayBackgroundView() override;

  // Called after the tray has been added to the widget containing it.
  virtual void Initialize();

  // Initializes animations for the bubble.
  static void InitializeBubbleAnimations(views::Widget* bubble_widget);

  // views::View:
  void SetVisible(bool visible) override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // ActionableView:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // TrayBubbleView::Delegate:
  void ProcessGestureEventForBubble(ui::GestureEvent* event) override;

  // VirtualKeyboardModel::Observer:
  void OnVirtualKeyboardVisibilityChanged() override;

  // Returns the associated tray bubble view, if one exists. Otherwise returns
  // nullptr.
  virtual TrayBubbleView* GetBubbleView();

  // Closes the associated tray bubble view if it exists and is currently
  // showing.
  virtual void CloseBubble();

  // Shows the associated tray bubble if one exists. |show_by_click| indicates
  // whether the showing operation is initiated by mouse or gesture click.
  virtual void ShowBubble(bool show_by_click);

  // Called whenever the shelf alignment changes.
  virtual void UpdateAfterShelfAlignmentChange();

  // Called whenever the bounds of the root window changes.
  virtual void UpdateAfterRootWindowBoundsChange(const gfx::Rect& old_bounds,
                                                 const gfx::Rect& new_bounds);

  // Called when the anchor (tray or bubble) may have moved or changed.
  virtual void AnchorUpdated();

  // Called from GetAccessibleNodeData, must return a valid accessible name.
  virtual base::string16 GetAccessibleNameForTray() = 0;

  // Called when the bubble is resized.
  virtual void BubbleResized(const TrayBubbleView* bubble_view);

  // Hides the bubble associated with |bubble_view|. Called when the widget
  // is closed.
  virtual void HideBubbleWithView(const TrayBubbleView* bubble_view) = 0;

  // Called by the bubble wrapper when a click event occurs outside the bubble.
  // May close the bubble.
  virtual void ClickedOutsideBubble() = 0;

  // Returns the bubble anchor alignment based on |shelf_alignment_|.
  TrayBubbleView::AnchorAlignment GetAnchorAlignment() const;

  void SetIsActive(bool is_active);
  bool is_active() const { return is_active_; }

  TrayContainer* tray_container() const { return tray_container_; }
  TrayEventFilter* tray_event_filter() { return tray_event_filter_.get(); }
  Shelf* shelf() { return shelf_; }

  // Updates the arrow visibility based on the launcher visibility.
  void UpdateBubbleViewArrow(TrayBubbleView* bubble_view);

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfItemBackground(SkColor color) override;

  // Updates the visibility of this tray's separator.
  void set_separator_visibility(bool visible) { separator_visible_ = visible; }

  // Gets the anchor for bubbles, which is tray_container().
  views::View* GetBubbleAnchor() const;

  // Gets additional insets for positioning bubbles relative to
  // tray_container().
  gfx::Insets GetBubbleAnchorInsets() const;

  // Updates the |clipping_window_| bounds if the anchor moved or changed.
  void UpdateClippingWindowBounds();

  // Returns the container window for the bubble (on the proper display).
  aura::Window* GetBubbleWindowContainer();

  // Update the bounds of the associated tray bubble. Close the bubble if
  // |close_bubble| is set.
  void AnimateToTargetBounds(const gfx::Rect& target_bounds, bool close_bubble);

  // Helper function that calculates background bounds relative to local bounds
  // based on background insets returned from GetBackgroundInsets().
  gfx::Rect GetBackgroundBounds() const;

  aura::Window* clipping_window_for_test() const {
    return clipping_window_.get();
  }

  TrayDragController* drag_controller() { return drag_controller_.get(); }

 protected:
  // ActionableView:
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  bool PerformAction(const ui::Event& event) override;
  void HandlePerformActionResult(bool action_performed,
                                 const ui::Event& event) override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;

  void set_drag_controller(
      std::unique_ptr<TrayDragController> drag_controller) {
    drag_controller_ = std::move(drag_controller);
  }

  void set_show_with_virtual_keyboard(bool show_with_virtual_keyboard) {
    show_with_virtual_keyboard_ = show_with_virtual_keyboard;
  }

 private:
  class TrayWidgetObserver;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;
  bool RequiresNotificationWhenAnimatorDestroyed() const override;

  // Applies transformations to the |layer()| to animate the view when
  // SetVisible(false) is called.
  void HideTransformation();

  // Helper function that calculates background insets relative to local bounds.
  gfx::Insets GetBackgroundInsets() const;


  // The shelf containing the system tray for this view.
  Shelf* shelf_;

  // Convenience pointer to the contents view.
  TrayContainer* tray_container_;

  // Owned by the view passed to SetContents().
  TrayBackground* background_;

  // Determines if the view is active. This changes how  the ink drop ripples
  // behave.
  bool is_active_;

  // Visibility of this tray's separator which is a line of 1x32px and 4px to
  // right of tray.
  bool separator_visible_;

  // During virtual keyboard is shown, visibility changes to TrayBackgroundView
  // are ignored. In such case, preferred visibility is reflected after the
  // virtual keyboard is hidden.
  bool visible_preferred_;

  // If true, the view always shows up when virtual keyboard is visible.
  bool show_with_virtual_keyboard_;

  // Handles touch drag gestures on the tray area and its associated bubble.
  std::unique_ptr<TrayDragController> drag_controller_;

  // Used in maximize mode to make sure the system tray bubble only be shown in
  // work area.
  std::unique_ptr<aura::Window> clipping_window_;

  std::unique_ptr<TrayWidgetObserver> widget_observer_;
  std::unique_ptr<TrayEventFilter> tray_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
