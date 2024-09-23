// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_BASE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_BASE_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/overview/event_handler_delegate.h"
#include "ash/wm/overview/overview_types.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class RectF;
class RoundedCornersF;
}  // namespace gfx

namespace ui {
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

class DragWindowController;
class OverviewGrid;
class OverviewItem;
class OverviewSession;
class RoundedLabelWidget;
class SystemShadow;

// Defines the interface for the overview item which will be implemented by
// `OverviewItem`, `OverviewGroupItem` or `OverviewDropTarget`. The
// `OverviewGrid` creates and owns the top-level concrete instance of this
// interface.
class ASH_EXPORT OverviewItemBase : public EventHandlerDelegate {
 public:
  OverviewItemBase(OverviewSession* overview_session,
                   OverviewGrid* overview_grid,
                   aura::Window* root_window);
  OverviewItemBase(const OverviewItemBase&) = delete;
  OverviewItemBase& operator=(const OverviewItemBase&) = delete;
  ~OverviewItemBase() override;

  // Creates an instance of the `this` based on whether the given `window`
  // belongs to a snap group or not.
  static std::unique_ptr<OverviewItemBase> Create(
      aura::Window* window,
      OverviewSession* overview_session,
      OverviewGrid* overview_grid);

  void set_should_animate_when_entering(bool should_animate) {
    should_animate_when_entering_ = should_animate;
  }

  bool should_animate_when_entering() const {
    return should_animate_when_entering_;
  }

  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  void set_should_animate_when_exiting(bool should_animate) {
    should_animate_when_exiting_ = should_animate;
  }

  void set_should_restack_on_animation_end(bool val) {
    should_restack_on_animation_end_ = val;
  }

  aura::Window* root_window() { return root_window_; }
  const aura::Window* root_window() const { return root_window_; }

  OverviewGrid* overview_grid() { return overview_grid_; }

  views::Widget* item_widget() { return item_widget_.get(); }
  const views::Widget* item_widget() const { return item_widget_.get(); }

  const gfx::RectF& target_bounds() const { return target_bounds_; }

  bool is_moving_to_another_desk() const { return is_moving_to_another_desk_; }

  bool animating_to_close() const { return animating_to_close_; }

  void set_unclipped_size(std::optional<gfx::Size> unclipped_size) {
    unclipped_size_ = unclipped_size;
  }

  void set_scrolling_bounds(std::optional<gfx::RectF> scrolling_bounds) {
    scrolling_bounds_ = scrolling_bounds;
  }

  std::optional<gfx::RectF> scrolling_bounds() const {
    return scrolling_bounds_;
  }

  bool should_use_spawn_animation() const {
    return should_use_spawn_animation_;
  }

  // Returns true if `this` is currently being dragged.
  bool IsDragItem() const;

  // Shows/Hides window item during window dragging. Used when swiping up a
  // window from shelf.
  void SetVisibleDuringItemDragging(bool visible, bool animate);

  // Refreshes visuals of the `shadow_` by setting the visibility and updating
  // the bounds.
  void RefreshShadowVisuals(bool shadow_visible);

  // Updates the type for the `shadow_` while being dragged and dropped.
  void UpdateShadowTypeForDrag(bool is_dragging);

  // If in tablet mode, maybe forward events to `OverviewGridEventHandler` as we
  // might want to process scroll events on `this`. `event_source_item`
  // specifies the sender of the event.
  void HandleGestureEventForTabletModeLayout(
      ui::GestureEvent* event,
      OverviewItemBase* event_source_item);

  // Updates the opacity of `item_widget_`, all the window(s) owned by `this`
  // and `cannot_snap_widget_`.
  virtual void SetOpacity(float opacity);

  // Returns the list of windows that we want to slide up or down when swiping
  // on the shelf in tablet mode.
  virtual aura::Window::Windows GetWindowsForHomeGesture();

  // Hides the overview item. This is used to hide any overview items that may
  // be present when entering the saved desk library. Animates `item_widget_`
  // and the windows in the transient tree to 0 opacity if `animate` is true,
  // otherwise just sets them to 0 opacity.
  virtual void HideForSavedDeskLibrary(bool animate);

  // Re-shows overview items that were hidden by the saved desk library. Called
  // when exiting the saved desk library and going back to the overview grid.
  // Fades the overview items in if `animate` is true, otherwise shows them
  // immediately.
  virtual void RevertHideForSavedDeskLibrary(bool animate);

  // Updates and maybe creates the mirrors needed for multi-display dragging.
  virtual void UpdateMirrorsForDragging(bool is_touch_dragging);

  // Resets the mirrors needed for multi display dragging.
  virtual void DestroyMirrorsForDragging();

  // Returns the window associated with this, which can be a single window or
  // a list of windows.
  // TODO(michelefan): This is temporarily added to reduce the scope of the
  // task, which will be replaced by `GetWindows()` in a follow-up cl.
  virtual aura::Window* GetWindow() = 0;

  // Returns the window(s) associated with this, which can be a single window or
  // a list of windows.
  virtual std::vector<raw_ptr<aura::Window, VectorExperimental>>
  GetWindows() = 0;

  // Returns true if all the windows represented by `this` are visible on all
  // workspaces.
  virtual bool HasVisibleOnAllDesksWindow() = 0;

  // Returns true if the given `target` is contained within `this`.
  virtual bool Contains(const aura::Window* target) const = 0;

  // Returns the direct `OverviewItem` that represents the given `window`. This
  // is temporarily added for the current overview tests, we should avoid using
  // this API moving forward.
  // TODO(b/297580539): Completely get rid of this API.
  virtual OverviewItem* GetLeafItemForWindow(aura::Window* window) = 0;

  // Restores and animates the managed window(s) to its non overview mode state.
  // Animates if `animate` is true. If `reset_transform` equals true, the
  // window's transform will be reset to identity transform when exiting
  // overview mode. It's needed when dragging an Arc app window in overview mode
  // to put it in split screen. In this case the restore of its transform needs
  // to be deferred until the Arc app window is snapped successfully, otherwise
  // the animation will look very ugly (the Arc app window enlarges itself to
  // maximized window bounds and then shrinks to its snapped window bounds).
  // Note if the window's transform is not reset here, it must be reset by
  // someone else at some point.
  virtual void RestoreWindow(bool reset_transform, bool animate) = 0;

  // Sets the bounds of `this` to `target_bounds` in the `root_window_`. The
  // bounds change will be animated as specified by `animation_type`.
  virtual void SetBounds(const gfx::RectF& target_bounds,
                         OverviewAnimationType animation_type) = 0;

  // TODO(http://b/297923747): Integrate continuous animation with snap groups.
  virtual gfx::Transform ComputeTargetTransform(
      const gfx::RectF& target_bounds) = 0;

  // Returns the union of the original target bounds of all transformed windows
  // represented by `this`, i.e. all regular (normal or transient descendants of
  // the windows returned by `GetWindows()`).
  virtual gfx::RectF GetWindowsUnionScreenBounds() const = 0;

  // Returns the `target_bounds_` of the `this` with insets of the header.
  virtual gfx::RectF GetTargetBoundsWithInsets() const = 0;

  // Returns the transformed bound of `this`.
  virtual gfx::RectF GetTransformedBounds() const = 0;

  // Calculates and returns an optimal scale ratio. Only the given `height` is
  // taken into account as the width can vary.
  virtual float GetItemScale(int height) = 0;

  // Increases the bounds of the dragged item.
  virtual void ScaleUpSelectedItem(OverviewAnimationType animation_type) = 0;

  // Ensures that a possibly minimized window becomes visible after restore.
  virtual void EnsureVisible() = 0;

  // Returns the focusable widgets contained in `this`.
  virtual std::vector<views::Widget*> GetFocusableWidgets() = 0;

  // Returns the backdrop view of `this`.
  virtual views::View* GetBackDropView() const = 0;

  // Returns true if `shadow_` should be created on the item, false otherwise.
  virtual bool ShouldHaveShadow() const = 0;

  // Updates the rounded corners and shadow on `this`.
  virtual void UpdateRoundedCornersAndShadow() = 0;

  virtual float GetOpacity() const = 0;

  // Dispatched before entering overview.
  virtual void PrepareForOverview() = 0;

  virtual void SetShouldUseSpawnAnimation(bool value) = 0;

  // Called when the starting animation is completed, or called immediately
  // if there was no starting animation to do any necessary visual changes.
  virtual void OnStartingAnimationComplete() = 0;

  // Inserts the item back to its original stacking order so that the order of
  // overview items is the same as when entering overview.
  virtual void Restack() = 0;

  // Called before dragging. Scales up the windows(s) hosted by `this` a little
  // bit to indicate its selection and stacks the window(s) at the top of the Z
  // order in order to keep them visible while being dragged around.
  virtual void StartDrag() = 0;

  virtual void OnOverviewItemDragStarted() = 0;
  virtual void OnOverviewItemDragEnded(bool snap) = 0;

  // Called when performing the continuous scroll on overview item to set
  // transform and opacity with pre-calculated `target_transform`.
  virtual void OnOverviewItemContinuousScroll(
      const gfx::Transform& target_transform,
      float scroll_ratio) = 0;

  // Shows the cannot snap warning if currently in splitview, and the associated
  // item cannot be snapped.
  virtual void UpdateCannotSnapWarningVisibility(bool animate) = 0;

  // Hides the cannot snap warning (if it was showing) until the next call to
  // `UpdateCannotSnapWarningVisibility`.
  virtual void HideCannotSnapWarning(bool animate) = 0;

  // Called when `this` is dragged and dropped on the mini view of another
  // desk, which prepares `this` for being removed from the grid, and the
  // window(s) to restore its transform.
  virtual void OnMovingItemToAnotherDesk() = 0;

  // Called when the `OverviewGrid` shuts down to reset the `item_widget_` and
  // remove window(s) from `ScopedOverviewHideWindows`.
  virtual void Shutdown() = 0;

  // Slides `this` up or down and then closes the associated window(s). Used in
  // overview swipe to close.
  virtual void AnimateAndCloseItem(bool up) = 0;

  // Stops the current animation of `item_widget_`.
  virtual void StopWidgetAnimation() = 0;

  virtual OverviewItemFillMode GetOverviewItemFillMode() const = 0;

  // Updates the `OverviewItemFillMode` for this item.
  virtual void UpdateOverviewItemFillMode() = 0;

  virtual const gfx::RoundedCornersF GetRoundedCorners() const = 0;

  // EventHandlerDelegate:
  void HandleMouseEvent(const ui::MouseEvent& event,
                        OverviewItemBase* event_source_item) override;
  void HandleGestureEvent(ui::GestureEvent* event,
                          OverviewItemBase* event_source_item) override;

  void set_target_bounds_for_testing(const gfx::RectF& target_bounds) {
    target_bounds_ = target_bounds;
  }

  SystemShadow* shadow_for_testing() { return shadow_.get(); }

  gfx::Rect get_shadow_content_bounds_for_testing() const {
    return shadow_ ? shadow_.get()->GetContentBounds() : gfx::Rect();
  }

  DragWindowController* item_mirror_for_dragging_for_testing() {
    return item_mirror_for_dragging_.get();
  }

  RoundedLabelWidget* get_cannot_snap_widget_for_testing() {
    return cannot_snap_widget_.get();
  }

  const std::optional<gfx::Size>& unclipped_size_for_testing() const {
    return unclipped_size_;
  }

 protected:
  // Returns the widget init params needed to create the `item_widget_`.
  views::Widget::InitParams CreateOverviewItemWidgetParams(
      aura::Window* parent_window,
      const std::string& widget_name,
      bool accept_events) const;

  // Creates the `shadow_` and stacks the shadow layer to be at the bottom after
  // `item_widget_` has been created.
  void CreateShadow();

  // Drag event can be handled differently based on the concreate instance of
  // `this`. For `OverviewItem`, the drag will be on window-level. For
  // `OverviewGroupItem`, the drag will be on group-leve.
  virtual void HandleDragEvent(const gfx::PointF& location_in_screen);

  // The root window `this` is being displayed on.
  raw_ptr<aura::Window> root_window_;

  // Pointer to the overview session that owns the `overview_grid_` containing
  // `this`. Guaranteed to be non-null for the lifetime of `this`.
  const raw_ptr<OverviewSession> overview_session_;

  // Pointer to the `OverviewGrid` that contains `this`. Guaranteed to be
  // non-null for the lifetime of `this`.
  const raw_ptr<OverviewGrid> overview_grid_;

  bool prepared_for_overview_ = false;

  // A widget stacked under the window(s). The widget has `OverviewItemView` or
  // `OverviewGroupContainerView` as its contents view. The widget is backed by
  // a NOT_DRAWN layer since most of its surface is transparent.
  std::unique_ptr<views::Widget> item_widget_;

  // The target bounds `this` is fit within. When in splitview, `item_widget_`
  // is fit within these bounds, but the window itself is transformed to
  // `unclipped_size_`, and then clipped.
  gfx::RectF target_bounds_;

  // The shadow around `this`.
  std::unique_ptr<SystemShadow> shadow_;

  // True when `this` is dragged and dropped on another desk's mini view and the
  // transform needs to be restored immediately without any animations.
  bool is_moving_to_another_desk_ = false;

  // True if the window(s) are still alive so they can have a closing animation.
  // These windows should not be used in calculations for
  // `OverviewGrid::PositionWindows()`.
  bool animating_to_close_ = false;

  // True if `this` should animate during the entering animation.
  bool should_animate_when_entering_ = true;

  // True if `this` should animate during the exiting animation.
  bool should_animate_when_exiting_ = true;

  // True if we need to reorder the stacking order of the widgets after an
  // animation.
  bool should_restack_on_animation_end_ = false;

  // A widget with text that may show up on top of the window(s) to notify
  // users `this` cannot be snapped.
  std::unique_ptr<RoundedLabelWidget> cannot_snap_widget_;

  // Contains a value if there is a snapped window, or a window about to be
  // snapped (triggering a splitview preview area), which will be set when items
  // are positioned in OverviewGrid. `SetBounds()` calculates the actual bounds
  // of `this`, but we want to maintain the aspect ratio of the windows, whose
  // bounds are not set to split view size. In `OverviewItem::SetItemBounds()`,
  // to this value instead of `target_bounds_`, and then apply clipping on the
  // window to `target_bounds_`.
  std::optional<gfx::Size> unclipped_size_ = std::nullopt;

  // Cached bounds of `this` to avoid being calculated on each scroll update.
  // Will be nullopt unless a grid scroll is underway.
  std::optional<gfx::RectF> scrolling_bounds_ = std::nullopt;

  // True if `this` should be added to an active overview session using the
  // spawn animation on its first update, which implies an animation type of
  // `OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW`. False otherwise. Reset the
  // value to false on spawn animation completed.
  bool should_use_spawn_animation_ = false;

 private:
  friend class OverviewTestBase;

  void HideItemWidgetWindow();
  void ShowItemWidgetWindow();

  // TODO(sammiequon): Current events go from `OverviewItemView` to
  // `EventHandlerDelegate` to `OverviewSession` to
  // `OverviewWindowDragController`. We may be able to shorten this pipeline.
  void HandlePressEvent(const gfx::PointF& location_in_screen,
                        bool from_touch_gesture,
                        OverviewItemBase* event_source_item);
  void HandleReleaseEvent(const gfx::PointF& location_in_screen);
  void HandleLongPressEvent(const gfx::PointF& location_in_screen);
  void HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                             float velocity_x,
                             float velocity_y);
  void HandleTapEvent(const gfx::PointF& location_in_screen,
                      OverviewItemBase* event_source_item);
  void HandleGestureEndEvent();

  // Cancellable callback to ensure that we are not going to hide the window
  // after reverting the hide.
  base::CancelableOnceClosure hide_window_in_overview_callback_;

  // Used to block events from reaching the item widget when the overview item
  // has been hidden.
  std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>
      item_widget_event_blocker_;

  // Responsible for mirrors that look like the `item_widget_` on all displays
  // during dragging.
  std::unique_ptr<DragWindowController> item_mirror_for_dragging_;

  base::WeakPtrFactory<OverviewItemBase> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_BASE_H_
