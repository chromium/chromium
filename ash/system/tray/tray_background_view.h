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
#include "ash/system/user/login_status.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {
class Shelf;
class TrayContainer;
class TrayEventFilter;

// Base class for some children of StatusAreaWidget. This class handles setting
// and animating the background when the Launcher is shown/hidden. It also
// inherits from ActionableView so that the tray items can override
// PerformAction when clicked on.
class ASH_EXPORT TrayBackgroundView : public ActionableView,
                                      public ui::LayerAnimationObserver,
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

  // ActionableView:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

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

  // Calculates the ideal bounds that this view should have depending on the
  // constraints.
  virtual void CalculateTargetBounds();

  // Makes this view's bounds and layout match its calculated target bounds.
  virtual void UpdateLayout();

  // Called to update the tray button after the login status changes.
  virtual void UpdateAfterLoginStatusChange();

  // Called whenever the status area's collapse state changes.
  virtual void UpdateAfterStatusAreaCollapseChange();

  // Called when the anchor (tray or bubble) may have moved or changed.
  virtual void AnchorUpdated() {}

  // Called from GetAccessibleNodeData, must return a valid accessible name.
  virtual base::string16 GetAccessibleNameForTray() = 0;

  // Called when a locale change is detected. It should reload any strings the
  // view may be using. Note that the locale is not expected to change after the
  // user logs in.
  virtual void HandleLocaleChange() = 0;

  // Called when the bubble is resized.
  virtual void BubbleResized(const TrayBubbleView* bubble_view);

  // Hides the bubble associated with |bubble_view|. Called when the widget
  // is closed.
  virtual void HideBubbleWithView(const TrayBubbleView* bubble_view) = 0;

  // Called by the bubble wrapper when a click event occurs outside the bubble.
  // May close the bubble.
  virtual void ClickedOutsideBubble() = 0;

  // Updates the background layer.
  virtual void UpdateBackground();

  void SetIsActive(bool is_active);
  bool is_active() const { return is_active_; }

  TrayContainer* tray_container() const { return tray_container_; }
  TrayEventFilter* tray_event_filter() { return tray_event_filter_.get(); }
  Shelf* shelf() { return shelf_; }

  // Updates the arrow visibility based on the launcher visibility.
  void UpdateBubbleViewArrow(TrayBubbleView* bubble_view);

  // Updates the visibility of this tray's separator.
  void set_separator_visibility(bool visible) { separator_visible_ = visible; }

  // Sets whether to show the view when the status area is collapsed.
  void set_show_when_collapsed(bool show_when_collapsed) {
    show_when_collapsed_ = show_when_collapsed;
  }

  // Gets the anchor for bubbles, which is tray_container().
  views::View* GetBubbleAnchor() const;

  // Gets additional insets for positioning bubbles relative to
  // tray_container().
  gfx::Insets GetBubbleAnchorInsets() const;

  // Returns the container window for the bubble (on the proper display).
  aura::Window* GetBubbleWindowContainer();

  // Helper function that calculates background bounds relative to local bounds
  // based on background insets returned from GetBackgroundInsets().
  gfx::Rect GetBackgroundBounds() const;

  // Sets whether the tray item should be shown by default (e.g. it is
  // activated). The effective visibility of the tray item is determined by the
  // current state of the status tray (i.e. whether the virtual keyboard is
  // showing or if it is collapsed).
  virtual void SetVisiblePreferred(bool visible_preferred);
  bool visible_preferred() const { return visible_preferred_; }

 protected:
  // ActionableView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  bool PerformAction(const ui::Event& event) override;
  void HandlePerformActionResult(bool action_performed,
                                 const ui::Event& event) override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;

  void set_show_with_virtual_keyboard(bool show_with_virtual_keyboard) {
    show_with_virtual_keyboard_ = show_with_virtual_keyboard;
  }

  void set_use_bounce_in_animation(bool use_bounce_in_animation) {
    use_bounce_in_animation_ = use_bounce_in_animation;
  }

 private:
  class TrayWidgetObserver;

  void StartVisibilityAnimation(bool visible);

  // views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  const char* GetClassName() const override;

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  // Applies transformations to the |layer()| to animate the view when
  // SetVisible(false) is called.
  void HideAnimation();
  void FadeInAnimation();
  void BounceInAnimation();

  // Helper function that calculates background insets relative to local bounds.
  gfx::Insets GetBackgroundInsets() const;

  // Returns the effective visibility of the tray item based on the current
  // state.
  bool GetEffectiveVisibility();

  // The shelf containing the system tray for this view.
  Shelf* shelf_;

  // Convenience pointer to the contents view.
  TrayContainer* tray_container_;

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

  // If true, the view is visible when the status area is collapsed.
  bool show_when_collapsed_;

  bool use_bounce_in_animation_ = false;

  std::unique_ptr<TrayWidgetObserver> widget_observer_;
  std::unique_ptr<TrayEventFilter> tray_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
