// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/user/login_status.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/context_menu_controller.h"

namespace ui {
class Event;
enum MenuSourceType;
}  // namespace ui

namespace views {
class AnimationAbortHandle;
class MenuRunner;
class View;
}  // namespace views

namespace ash {
class Shelf;
class TrayContainer;
class TrayEventFilter;

// Base class for some children of StatusAreaWidget. This class handles setting
// and animating the background when the Launcher is shown/hidden. It also
// inherits from ActionableView so that the tray items can override
// PerformAction when clicked on. Note that events targeting a
// `TrayBackgroundView`'s view hierarchy are ignored while the
// `TrayBackgroundView`'s hide animation is running.
class ASH_EXPORT TrayBackgroundView : public ActionableView,
                                      public views::ContextMenuController,
                                      public ShelfBackgroundAnimatorObserver,
                                      public TrayBubbleView::Delegate,
                                      public VirtualKeyboardModel::Observer {
 public:
  METADATA_HEADER(TrayBackgroundView);

  // Inherit from this class to be notified of events that happen for a specific
  // `TrayBackgroundView`.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the `TrayBackgroundView`'s preferred visibility changes.
    // `visible_preferred` is the new preferred visibility.
    virtual void OnVisiblePreferredChanged(bool visible_preferred) = 0;
  };

  enum RoundedCornerBehavior {
    kNotRounded,
    kStartRounded,
    kEndRounded,
    kAllRounded
  };

  TrayBackgroundView(Shelf* shelf,
                     const TrayBackgroundViewCatalogName catalog_name,
                     RoundedCornerBehavior corner_behavior = kAllRounded);
  TrayBackgroundView(const TrayBackgroundView&) = delete;
  TrayBackgroundView& operator=(const TrayBackgroundView&) = delete;
  ~TrayBackgroundView() override;

  void AddTrayBackgroundViewObserver(Observer* observer);
  void RemoveTrayBackgroundViewObserver(Observer* observer);

  // Overrides default button press handling in `PerformAction()`.
  void SetPressedCallback(
      base::RepeatingCallback<void(const ui::Event& event)> pressed_callback);

  // Called after the tray has been added to the widget containing it.
  virtual void Initialize();

  // Initializes animations for the bubble. This contains only a fade out
  // animation that hides `bubble_widget` when it becomes invisible.
  static void InitializeBubbleAnimations(views::Widget* bubble_widget);

  // ActionableView:
  void OnThemeChanged() override;

  // VirtualKeyboardModel::Observer:
  void OnVirtualKeyboardVisibilityChanged() override;

  // Returns the associated tray bubble view, if one exists. Otherwise returns
  // nullptr.
  virtual TrayBubbleView* GetBubbleView();

  // Returns the associated tray bubble widget, if a bubble exists. Otherwise
  // returns nullptr.
  virtual views::Widget* GetBubbleWidget() const;

  // Returns a lock that prevents window activation from closing bubbles.
  [[nodiscard]] static base::ScopedClosureRunner
  DisableCloseBubbleOnWindowActivated();

  // Whether a window activation change should close bubbles.
  static bool ShouldCloseBubbleOnWindowActivated();

  // Closes the associated tray bubble view if it exists and is currently
  // showing.
  virtual void CloseBubble() {}

  // Shows the associated tray bubble if one exists.
  virtual void ShowBubble();

  // Calculates the ideal bounds that this view should have depending on the
  // constraints.
  virtual void CalculateTargetBounds();

  // Makes this view's bounds and layout match its calculated target bounds.
  virtual void UpdateLayout();

  // Called to update the tray button after the login status changes.
  virtual void UpdateAfterLoginStatusChange();

  // Called whenever the lock state changes. `locked` represents the current
  // lock state.
  void UpdateAfterLockStateChange(bool locked);

  // Called whenever the status area's collapse state changes.
  virtual void UpdateAfterStatusAreaCollapseChange();

  // Called when the anchor (tray or bubble) may have moved or changed.
  virtual void AnchorUpdated() {}

  // Called after the tray has been activated, and `PerformAction()` has been
  // called.
  virtual void OnTrayActivated(const ui::Event& event);

  // Called from GetAccessibleNodeData, must return a valid accessible name.
  virtual std::u16string GetAccessibleNameForTray() = 0;

  // Called when a locale change is detected. It should reload any strings the
  // view may be using. Note that the locale is not expected to change after the
  // user logs in.
  virtual void HandleLocaleChange() = 0;

  // Updates this bubble about visibility change of *ANY* tray bubble
  // including itself.
  // `bubble_widget` is the bubble with visibility change. Please note that it
  // can be the current bubble as well.
  virtual void OnAnyBubbleVisibilityChanged(views::Widget* bubble_widget,
                                            bool visible);

  // Hides the bubble associated with |bubble_view|. Called when the widget
  // is closed.
  virtual void HideBubbleWithView(const TrayBubbleView* bubble_view) = 0;

  // Called by the bubble wrapper when a click event occurs outside the bubble.
  // May close the bubble.
  virtual void ClickedOutsideBubble() = 0;

  // Returns true if tray bubble view is cached when hidden
  virtual bool CacheBubbleViewForHide() const;

  // Updates the background layer.
  virtual void UpdateBackground();

  // For Jelly: updates the color of either the icon or the label of this view
  // based on the active state specified by `is_active`.
  virtual void UpdateTrayItemColor(bool is_active) = 0;

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

  // ActionableView:
  bool PerformAction(const ui::Event& event) final;

  // Sets whether the tray item should be shown by default (e.g. it is
  // activated). The effective visibility of the tray item is determined by the
  // current state of the status tray (i.e. whether the virtual keyboard is
  // showing or if it is collapsed).
  virtual void SetVisiblePreferred(bool visible_preferred);
  bool visible_preferred() const { return visible_preferred_; }

  // Disables bounce in and fade in animation. The animation will remain
  // disabled until the returned scoped closure runner is run.
  [[nodiscard]] base::ScopedClosureRunner DisableShowAnimation();

  // Registers a client's request to use custom visibility animations. The
  // custom animation must be executed by the client; `TrayBackgroundView` does
  // not run any custom animations (it will simply do nothing rather than run
  // the default visibility animations). Custom animations will be used until
  // the returned scoped closure runner is run.
  [[nodiscard]] base::ScopedClosureRunner SetUseCustomVisibilityAnimations();

  // Returns true if the view is showing a context menu.
  bool IsShowingMenu() const;

  // Set the rounded corner behavior for this tray item.
  void SetRoundedCornerBehavior(RoundedCornerBehavior corner_behavior);

  // Returns the corners based on the `corner_behavior_`;
  gfx::RoundedCornersF GetRoundedCorners();

  // Returns a weak pointer to this instance.
  base::WeakPtr<TrayBackgroundView> GetWeakPtr();

  // Checks if we should show bounce in or fade in animation.
  bool IsShowAnimationEnabled();

  // Callbacks for Animations
  void OnAnimationAborted();
  virtual void OnAnimationEnded();
  void OnHideAnimationStarted();

  void SetIsActive(bool is_active);
  bool is_active() const { return is_active_; }

  TrayContainer* tray_container() const { return tray_container_; }
  TrayEventFilter* tray_event_filter() { return tray_event_filter_.get(); }
  Shelf* shelf() { return shelf_; }
  TrayBackgroundViewCatalogName catalog_name() const { return catalog_name_; }

  // Updates the visibility of this tray's separator.
  void set_separator_visibility(bool visible) { separator_visible_ = visible; }

  // Sets whether to show the view when the status area is collapsed.
  void set_show_when_collapsed(bool show_when_collapsed) {
    show_when_collapsed_ = show_when_collapsed;
  }

  // Sets whether changes in lock state should cause this tray's bubble to close
  // if it is currently open.
  void set_should_close_bubble_on_lock_state_change(
      bool should_close_bubble_on_lock_state_change) {
    should_close_bubble_on_lock_state_change_ =
        should_close_bubble_on_lock_state_change;
  }

 protected:
  // ActionableView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  void HandlePerformActionResult(bool action_performed,
                                 const ui::Event& event) override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;

  virtual void OnShouldShowAnimationChanged(bool should_animate) {}

  // Specifies the menu that appears when this tray is right-clicked if
  // `SetContextMenuEnabled(true)` has been called. Default implementation
  // returns a nullptr, in which case no context menu is shown.
  virtual std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel();

  // After hide animation is finished/aborted/removed, we will need to do an
  // update to the view's visibility and the view's status area widget state.
  virtual void OnVisibilityAnimationFinished(bool should_log_visible_pod_count,
                                             bool aborted);

  // Used to start and stop pulse animation on tray button.
  void StartPulseAnimation();
  void StopPulseAnimation();

  void SetContextMenuEnabled(bool should_enable_menu) {
    set_context_menu_controller(should_enable_menu ? this : nullptr);
  }

  void set_show_with_virtual_keyboard(bool show_with_virtual_keyboard) {
    show_with_virtual_keyboard_ = show_with_virtual_keyboard;
  }

  void set_use_bounce_in_animation(bool use_bounce_in_animation) {
    use_bounce_in_animation_ = use_bounce_in_animation;
  }

 private:
  class TrayWidgetObserver;
  class TrayBackgroundViewSessionChangeHandler;
  friend class StatusAreaWidgetQSRevampTest;

  void StartVisibilityAnimation(bool visible);

  // Updates status area widget by calling `UpdateCollapseState()` and
  // `LogVisiblePodCountMetric()`.
  void UpdateStatusArea(bool should_log_visible_pod_count);

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  // In some cases, we expect the layer's visibility to be set to false right
  // away when the layer is replaced. See
  // `OverviewButtonTrayTest.HideAnimationAlwaysCompletesOnDelete` test as an
  // example. We use `::wm::RecreateLayers(root_window)` to create fresh layers
  // for the window. If we don't override this method, the old layer and its
  // child layers will still be there until all the animation finished.
  std::unique_ptr<ui::Layer> RecreateLayer() override;

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

  // Checks if we should use custom visibility animations.
  bool ShouldUseCustomVisibilityAnimations() const;

  // For Material Next: Updates the background color based on active state.
  void UpdateBackgroundColor(bool active);

  // Add and remove ripple_layer_ from parent.
  void AddRippleLayer();
  void RemoveRippleLayer();

  // The shelf containing the system tray for this view.
  raw_ptr<Shelf, ExperimentalAsh> shelf_;

  // The catalog name, used to record metrics on feature integrations.
  const TrayBackgroundViewCatalogName catalog_name_;

  // Convenience pointer to the contents view.
  raw_ptr<TrayContainer, ExperimentalAsh> tray_container_;

  // A separate layer for ripple aimation.
  std::unique_ptr<ui::Layer> ripple_layer_;
  // The handle to abort ripple and pulse animation.
  std::unique_ptr<views::AnimationAbortHandle>
      ripple_and_pulse_animation_abort_handle_;

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

  bool use_bounce_in_animation_ = true;
  bool is_starting_animation_ = false;

  // Number of active requests to disable the bounce-in and fade-in animation.
  size_t disable_show_animation_count_ = 0;

  // Number of active requests to use custom visibility animations.
  size_t use_custom_visibility_animation_count_ = 0;

  // The shape of this tray which is only applied to the horizontal tray.
  // Defaults to `kAllRounded`.
  RoundedCornerBehavior corner_behavior_;

  // Called instead of the default `PerformAction()`.
  base::RepeatingCallback<void(const ui::Event& event)> pressed_callback_;

  std::unique_ptr<TrayWidgetObserver> widget_observer_;
  std::unique_ptr<TrayEventFilter> tray_event_filter_;
  std::unique_ptr<TrayBackgroundViewSessionChangeHandler> handler_;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // Whether changes in lock state should cause this tray's bubble to close if
  // it is currently open.
  bool should_close_bubble_on_lock_state_change_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TrayBackgroundView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
