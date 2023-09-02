// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/window_state_observer.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

namespace ash {
class DragWindowController;
class OverviewGrid;
class OverviewItemView;
class OverviewSession;

// This class implements `OverviewItemBase` and represents a single window in
// overview mode. It handles placing the window in the correct bounds given by
// `OverviewGrid`, and owns a widget which contains an item's overview specific
// ui (title, icon, close button, etc.).
class ASH_EXPORT OverviewItem : public OverviewItemBase,
                                public aura::WindowObserver,
                                public WindowStateObserver {
 public:
  OverviewItem(aura::Window* window,
               OverviewSession* overview_session,
               OverviewGrid* overview_grid);

  OverviewItem(const OverviewItem&) = delete;
  OverviewItem& operator=(const OverviewItem&) = delete;

  ~OverviewItem() override;

  OverviewItemView* overview_item_view() { return overview_item_view_; }

  // If the window item represents a minimized window, update its contents view.
  void UpdateItemContentViewForMinimizedWindow();

  OverviewAnimationType GetExitOverviewAnimationType() const;
  OverviewAnimationType GetExitTransformAnimationType() const;

  // If in tablet mode, maybe forward events to `OverviewGridEventHandler` as we
  // might want to process scroll events on the item.
  void HandleGestureEventForTabletModeLayout(ui::GestureEvent* event);

  // OverviewItemBase:
  aura::Window* GetWindow() override;
  std::vector<aura::Window*> GetWindows() override;
  bool Contains(const aura::Window* target) const override;
  OverviewItem* GetLeafItemForWindow(aura::Window* window) override;
  void RestoreWindow(bool reset_transform, bool animate) override;
  void SetBounds(const gfx::RectF& target_bounds,
                 OverviewAnimationType animation_type) override;
  float GetItemScale(const gfx::Size& size) override;
  void ScaleUpSelectedItem(OverviewAnimationType animation_type) override;
  void EnsureVisible() override;
  gfx::RectF GetTargetBoundsInScreen() const override;
  gfx::RectF GetWindowTargetBoundsWithInsets() const override;
  gfx::RectF GetTransformedBounds() const override;
  OverviewFocusableView* GetFocusableView() const override;
  views::View* GetBackDropView() const override;
  void UpdateRoundedCornersAndShadow() override;
  void SetShadowBounds(absl::optional<gfx::RectF> bounds_in_screen) override;
  void SetOpacity(float opacity) override;
  float GetOpacity() const override;
  void PrepareForOverview() override;
  void OnStartingAnimationComplete() override;
  void HideForSavedDeskLibrary(bool animate) override;
  void RevertHideForSavedDeskLibrary(bool animate) override;
  void Restack() override;
  void CloseWindow() override;
  void HandleMouseEvent(const ui::MouseEvent& event) override;
  void HandleGestureEvent(ui::GestureEvent* event) override;
  void OnFocusedViewActivated() override;
  void OnFocusedViewClosed() override;
  void OnOverviewItemDragStarted(OverviewItemBase* item) override;
  void OnOverviewItemDragEnded(bool snap) override;
  void OnOverviewItemContinuousScroll(const gfx::RectF& target_bounds,
                                      bool first_scroll,
                                      float scroll_ratio) override;
  void SetVisibleDuringItemDragging(bool visible, bool animate) override;
  void UpdateShadowTypeForDrag(bool is_dragging) override;
  void UpdateCannotSnapWarningVisibility(bool animate) override;
  void HideCannotSnapWarning(bool animate) override;
  void OnMovingItemToAnotherDesk() override;
  void UpdateMirrorsForDragging(bool is_touch_dragging) override;
  void DestroyMirrorsForDragging() override;
  void Shutdown() override;
  void AnimateAndCloseItem(bool up) override;
  void StopWidgetAnimation() override;
  OverviewGridWindowFillMode GetWindowDimensionsType() const override;
  void UpdateWindowDimensionsType() override;
  void CreateItemWidget() override;
  gfx::Point GetMagnifierFocusPointInScreen() const override;

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

 private:
  friend class OverviewTestBase;
  FRIEND_TEST_ALL_PREFIXES(SplitViewOverviewSessionTest, Clipping);

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

  // Updates the |item_widget|'s bounds. Any change in bounds will be animated
  // from the current bounds to the new bounds as per the |animation_type|.
  void UpdateHeaderLayout(OverviewAnimationType animation_type);

  // Updates the bounds of `item_widget` if the feature flag Jellyroll is
  // enabled. Once the feature is fully launched, this function will be renamed
  // to `UpdateHeaderLayout` and the function above should be removed.
  void UpdateHeaderLayoutCrOSNext(OverviewAnimationType animation_type);

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
  raw_ptr<aura::Window, ExperimentalAsh> root_window_;

  // The contained Window's wrapper.
  ScopedOverviewTransformWindow transform_window_;

  // Used to block events from reaching the item widget when the overview item
  // has been hidden.
  std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>
      item_widget_event_blocker_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_ = false;

  // The view associated with |item_widget_|. Contains a title, close button and
  // maybe a backdrop. Forwards certain events to |this|.
  raw_ptr<OverviewItemView, DanglingUntriaged | ExperimentalAsh>
      overview_item_view_ = nullptr;

  // Responsible for mirrors that look like the window on all displays during
  // dragging.
  // TODO(sammiequon): We need two, one for the `item_widget_` and one for the
  // source window (if not minimized). If DragWindowController supports multiple
  // windows in the future, combine these.
  std::unique_ptr<DragWindowController> item_mirror_for_dragging_;
  std::unique_ptr<DragWindowController> window_mirror_for_dragging_;

  // True if the windows are still alive so they can have a closing animation.
  // These windows should not be used in calculations for
  // OverviewGrid::PositionWindows.
  bool animating_to_close_ = false;

  // True if this overview item is currently being dragged around.
  bool is_being_dragged_ = false;

  bool prepared_for_overview_ = false;

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
