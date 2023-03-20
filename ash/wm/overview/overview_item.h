// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/window_state_observer.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {
class DragWindowController;
class OverviewGrid;
class OverviewItemView;
class RoundedLabelWidget;
class SystemShadow;

// This class represents an item in overview mode.
class ASH_EXPORT OverviewItem : public aura::WindowObserver,
                                public WindowStateObserver {
 public:
  OverviewItem(aura::Window* window,
               OverviewSession* overview,
               OverviewGrid* overview_grid);

  OverviewItem(const OverviewItem&) = delete;
  OverviewItem& operator=(const OverviewItem&) = delete;

  ~OverviewItem() override;

  SystemShadow* shadow() { return shadow_.get(); }

  aura::Window* GetWindow();

  // Returns true if |target| is contained in this OverviewItem.
  bool Contains(const aura::Window* target) const;

  // This called when the window is dragged and dropped on the mini view of
  // another desk, which prepares this item for being removed from the grid, and
  // the window to restore its transform.
  void OnMovingWindowToAnotherDesk();

  // Restores and animates the managed window to its non overview mode state.
  // Doesn't animate if |was_saved_desk_library_showing| is true. If
  // |reset_transform| equals false, the window's transform will not be reset to
  // identity transform when exiting overview mode. It's needed when dragging an
  // Arc app window in overview mode to put it in split screen. In this case the
  // restore of its transform needs to be deferred until the Arc app window is
  // snapped successfully, otherwise the animation will look very ugly (the Arc
  // app window enlarges itself to maximized window bounds and then shrinks to
  // its snapped window bounds). Note if the window's transform is not reset
  // here, it must be reset by someone else at some point.
  void RestoreWindow(bool reset_transform,
                     bool was_saved_desk_library_showing = false);

  // Ensures that a possibly minimized window becomes visible after restore.
  void EnsureVisible();

  // Restores stacking of window captions above the windows, then fades out.
  void Shutdown();

  // Hides the overview item. This is used to hide any overview items that may
  // be present when entering the saved desk library. Animates `item_widget_`
  // and the windows in the transient tree to 0 opacity if `animate` is true,
  // otherwise just sets them to 0 opacity.
  void HideForSavedDeskLibrary(bool animate);

  // This shows overview items that were hidden by the saved desk library.
  // Called when exiting the saved desk library and going back to the overview
  // grid. Fades the overview items in if `animate` is true, otherwise shows
  // them immediately.
  void RevertHideForSavedDeskLibrary(bool animate);

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  void PrepareForOverview();

  // Calculates and returns an optimal scale ratio. With MD this is only
  // taking into account |size.height()| as the width can vary. Without MD this
  // returns the scale that allows the item to fully fit within |size|.
  float GetItemScale(const gfx::Size& size);

  // Returns the union of the original target bounds of all transformed windows
  // managed by |this| item, i.e. all regular (normal or panel transient
  // descendants of the window returned by GetWindow()).
  gfx::RectF GetTargetBoundsInScreen() const;

  // Returns the transformed bound of |transform_window_|.
  gfx::RectF GetTransformedBounds() const;

  // Returns the target bounds of `window_`. Same as `target_bounds_`, with some
  // insets.
  gfx::RectF GetWindowTargetBoundsWithInsets() const;

  // Sets the bounds of this overview item to |target_bounds| in the
  // |root_window_| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(const gfx::RectF& target_bounds,
                 OverviewAnimationType animation_type);

  // Sends an accessibility event indicating that this window became selected
  // so that it is highlighted and announced.
  void SendAccessibleSelectionEvent();

  // Slides the item up or down and then closes the associated window. Used by
  // overview swipe to close.
  void AnimateAndCloseWindow(bool up);

  // Closes |transform_window_|.
  void CloseWindow();

  // Shows the cannot snap warning if currently in splitview, and the associated
  // window cannot be snapped.
  void UpdateCannotSnapWarningVisibility(bool animate);

  // Hides the cannot snap warning (if it was showing) until the next call to
  // |UpdateCannotSnapWarningVisibility|.
  void HideCannotSnapWarning(bool animate);

  // Called when a OverviewItem on any grid is dragged. Hides the close button
  // when a drag is started, and reshows it when a drag is finished.
  // Additionally hides the title and window icon if |item| is this.
  void OnSelectorItemDragStarted(OverviewItem* item);
  void OnSelectorItemDragEnded(bool snap);

  // Shows/Hides window item during window dragging. Used when swiping up a
  // window from shelf.
  void SetVisibleDuringWindowDragging(bool visible, bool animate);

  OverviewGridWindowFillMode GetWindowDimensionsType() const;

  // Recalculates the window dimensions type of |transform_window_|. Called when
  // |window_|'s bounds change.
  void UpdateWindowDimensionsType();

  // TODO(minch): Do not actually scale up the item to get the bounds.
  // http://crbug.com/876567.
  // Returns the bounds of the selected item, which will be scaled up a little
  // bit and header view will be hidden after being selected. Note, the item
  // will be restored back after scaled up.
  gfx::Rect GetBoundsOfSelectedItem();

  // Increases the bounds of the dragged item.
  void ScaleUpSelectedItem(OverviewAnimationType animation_type);

  // If the window item represents a minimized window, update its content view.
  void UpdateItemContentViewForMinimizedWindow();

  // Checks if this item is currently being dragged.
  bool IsDragItem();

  // Inserts the window back to its original stacking order so that the order of
  // windows is the same as when entering overview.
  void Restack();

  // Updates |phantoms_for_dragging_|. If |phantoms_for_dragging_| is null, then
  // a new object is created for it.
  void UpdatePhantomsForDragging(bool is_touch_dragging);

  void DestroyPhantomsForDragging();

  // Sets the bounds of the window shadow. If |bounds_in_screen| is nullopt,
  // the shadow is hidden.
  void SetShadowBounds(absl::optional<gfx::RectF> bounds_in_screen);

  // Updates the rounded corners and shadow on this overview window item.
  void UpdateRoundedCornersAndShadow();

  // Updates the shadow type while being dragged and dropped.
  void UpdateShadowTypeForDrag(bool is_dragging);

  // Called when the starting animation is completed, or called immediately
  // if there was no starting animation.
  void OnStartingAnimationComplete();

  // Stops the current animation of |item_widget_|.
  void StopWidgetAnimation();

  // Changes the opacity of all the windows the item owns.
  void SetOpacity(float opacity);
  float GetOpacity();

  OverviewAnimationType GetExitOverviewAnimationType();
  OverviewAnimationType GetExitTransformAnimationType();

  // If kNewOverviewLayout is on, use this function for handling events.
  void HandleGestureEventForTabletModeLayout(ui::GestureEvent* event);

  // Handles events forwarded from |overview_item_view_|.
  void HandleMouseEvent(const ui::MouseEvent& event);
  void HandleGestureEvent(ui::GestureEvent* event);
  void OnHighlightedViewActivated();
  void OnHighlightedViewClosed();

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // Returns the root window on which this item is shown.
  aura::Window* root_window() { return root_window_; }

  const gfx::RectF& target_bounds() const { return target_bounds_; }

  views::Widget* item_widget() { return item_widget_.get(); }

  OverviewItemView* overview_item_view() { return overview_item_view_; }

  OverviewGrid* overview_grid() { return overview_grid_; }

  bool is_moving_to_another_desk() const { return is_moving_to_another_desk_; }

  void set_should_use_spawn_animation(bool value) {
    should_use_spawn_animation_ = value;
  }
  bool should_use_spawn_animation() const {
    return should_use_spawn_animation_;
  }

  void set_should_animate_when_entering(bool should_animate) {
    should_animate_when_entering_ = should_animate;
  }
  bool should_animate_when_entering() const {
    return should_animate_when_entering_;
  }

  void set_should_animate_when_exiting(bool should_animate) {
    should_animate_when_exiting_ = should_animate;
  }
  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  void set_should_restack_on_animation_end(bool val) {
    should_restack_on_animation_end_ = val;
  }

  bool animating_to_close() const { return animating_to_close_; }

  void set_unclipped_size(absl::optional<gfx::Size> unclipped_size) {
    unclipped_size_ = unclipped_size;
  }

  void set_scrolling_bounds(absl::optional<gfx::RectF> scrolling_bounds) {
    scrolling_bounds_ = scrolling_bounds;
  }
  absl::optional<gfx::RectF> scrolling_bounds() const {
    return scrolling_bounds_;
  }

  gfx::Rect GetShadowBoundsForTesting();
  RoundedLabelWidget* cannot_snap_widget_for_testing() {
    return cannot_snap_widget_.get();
  }

  void set_target_bounds_for_testing(const gfx::RectF& target_bounds) {
    target_bounds_ = target_bounds;
  }
  void set_animating_to_close_for_testing(bool val) {
    animating_to_close_ = val;
  }

 private:
  friend class OverviewTestBase;
  FRIEND_TEST_ALL_PREFIXES(SplitViewOverviewSessionTest, Clipping);

  // The shadow should match the size of the transformed window or preview
  // window if unclipped.
  gfx::RectF GetUnclippedShadowBounds() const;

  // Functions to be called back when their associated animations complete.
  void OnWindowCloseAnimationCompleted();
  void OnItemSpawnedAnimationCompleted();
  void OnItemBoundsAnimationStarted();
  void OnItemBoundsAnimationEnded();

  // Performs the spawn-item-in-overview animation (which is a fade-in plus
  // scale-up animation), on the given |window|. |target_transform| is the final
  // transform that should be applied to |window| at the end of the animation.
  // |window| is either the real window associated with this item (from
  // GetWindow()), or the `item_widget_->GetNativeWindow()` if the associated
  // window is minimized.
  void PerformItemSpawnedAnimation(aura::Window* window,
                                   const gfx::Transform& target_transform);

  // Sets the bounds of this overview item to |target_bounds| in |root_window_|.
  // The bounds change will be animated as specified by |animation_type|.
  // |is_first_update| is true when we set this item's bounds for the first
  // time.
  void SetItemBounds(const gfx::RectF& target_bounds,
                     OverviewAnimationType animation_type,
                     bool is_first_update);

  // Creates |item_widget_|, which holds |overview_item_view_|.
  void CreateItemWidget();

  // Updates the |item_widget|'s bounds. Any change in bounds will be animated
  // from the current bounds to the new bounds as per the |animation_type|.
  void UpdateHeaderLayout(OverviewAnimationType animation_type);

  // Animates opacity of the |transform_window_| and its caption to |opacity|
  // using |animation_type|.
  void AnimateOpacity(float opacity, OverviewAnimationType animation_type);

  // Returns the type of animation to use for an item that manages a minimized
  // window.
  OverviewAnimationType GetExitOverviewAnimationTypeForMinimizedWindow(
      OverviewEnterExitType type);

  // Called before dragging. Scales up the window a little bit to indicate its
  // selection and stacks the window at the top of the Z order in order to keep
  // it visible while dragging around.
  void StartDrag();

  void CloseButtonPressed();

  // TODO(sammiequon): Current events go from OverviewItemView to
  // OverviewItem to OverviewSession to OverviewWindowDragController. We may be
  // able to shorten this pipeline.
  void HandlePressEvent(const gfx::PointF& location_in_screen,
                        bool from_touch_gesture);
  void HandleReleaseEvent(const gfx::PointF& location_in_screen);
  void HandleDragEvent(const gfx::PointF& location_in_screen);
  void HandleLongPressEvent(const gfx::PointF& location_in_screen);
  void HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                             float velocity_x,
                             float velocity_y);
  void HandleTapEvent();
  void HandleGestureEndEvent();

  void HideWindowInOverview();
  void ShowWindowInOverview();

  // Returns the list of windows that we want to slide up or down when swiping
  // on the shelf in tablet mode.
  aura::Window::Windows GetWindowsForHomeGesture();

  // The root window this item is being displayed on.
  aura::Window* root_window_;

  // The contained Window's wrapper.
  ScopedOverviewTransformWindow transform_window_;

  // Used to block events from reaching the item widget when the overview item
  // has been hidden.
  std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>
      item_widget_event_blocker_;

  // The target bounds this overview item is fit within. When in splitview,
  // |item_widget_| is fit within these bounds, but the window itself is
  // transformed to |unclipped_size_|, and then clipped.
  gfx::RectF target_bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_ = false;

  // A widget stacked under the |transform_window_|. The widget has
  // |overview_item_view_| as its contents view. The widget is backed by a
  // NOT_DRAWN layer since most of its surface is transparent.
  std::unique_ptr<views::Widget> item_widget_;

  // The view associated with |item_widget_|. Contains a title, close button and
  // maybe a backdrop. Forwards certain events to |this|.
  OverviewItemView* overview_item_view_ = nullptr;

  // A widget with text that may show up on top of |transform_window_| to notify
  // users this window cannot be snapped.
  std::unique_ptr<RoundedLabelWidget> cannot_snap_widget_;

  // Responsible for phantoms that look like the window on all displays during
  // dragging.
  std::unique_ptr<DragWindowController> phantoms_for_dragging_;

  // Pointer to the Overview that owns the OverviewGrid containing |this|.
  // Guaranteed to be non-null for the lifetime of |this|.
  OverviewSession* overview_session_;

  // Pointer to the OverviewGrid that contains |this|. Guaranteed to be non-null
  // for the lifetime of |this|.
  OverviewGrid* overview_grid_;

  // True when the item is dragged and dropped on another desk's mini view. This
  // causes it to restore its transform immediately without any animations,
  // since it is moving to an inactive desk, and therefore won't be visible.
  bool is_moving_to_another_desk_ = false;

  // True if this item should be added to an active overview session using the
  // spawn animation on its first update. This implies an animation type of
  // OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW. This value will be reset to
  // false once the spawn animation is performed.
  bool should_use_spawn_animation_ = false;

  // True if the contained window should animate during the entering animation.
  bool should_animate_when_entering_ = true;

  // True if the contained window should animate during the exiting animation.
  bool should_animate_when_exiting_ = true;

  // True if after an animation, we need to reorder the stacking order of the
  // widgets.
  bool should_restack_on_animation_end_ = false;

  // True if the windows are still alive so they can have a closing animation.
  // These windows should not be used in calculations for
  // OverviewGrid::PositionWindows.
  bool animating_to_close_ = false;

  // True if this overview item is currently being dragged around.
  bool is_being_dragged_ = false;

  bool prepared_for_overview_ = false;

  // This has a value when there is a snapped window, or a window about to be
  // snapped (triggering a splitview preview area). This will be set when items
  // are positioned in OverviewGrid. The bounds delivered in |SetBounds| are the
  // true bounds of this item, but we want to maintain the aspect ratio of the
  // window, who's bounds are not set to split view size. So in |SetItemBounds|,
  // we transform the window not to |target_bounds_| but to this value, and then
  // apply clipping on the window to |target_bounds_|.
  absl::optional<gfx::Size> unclipped_size_ = absl::nullopt;

  // The shadow around the overview window. Shadows the original window, not
  // |item_widget_|. Done here instead of on the original window because of the
  // rounded edges mask applied on entering overview window.
  std::unique_ptr<SystemShadow> shadow_;

  // Cached values of the item bounds so that they do not have to be calculated
  // on each scroll update. Will be nullopt unless a grid scroll is underway.
  absl::optional<gfx::RectF> scrolling_bounds_ = absl::nullopt;

  // Disable animations on the contained window while it is being managed by the
  // overview item.
  ScopedAnimationDisabler animation_disabler_;

  // Cancellable callback to ensure that we are not going to hide the window
  // after reverting the hide.
  base::CancelableOnceClosure hide_window_in_overview_callback_;

  base::WeakPtrFactory<OverviewItem> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
