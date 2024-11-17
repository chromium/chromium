// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/event_handler_delegate.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/window_state_observer.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/wm/core/scoped_animation_disabler.h"

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
  // Defines an interface for the deletage to `OverviewItem`, which will be
  // notified on the observed window represented by the overview item being
  // destroyed.
  class WindowDestructionDelegate {
   public:
    // Called when the observed window represented by the `overview_item` is
    // being destroyed. `reposition` is true if the overview item needs to be
    // repositioned and the grid bounds need to be updated.
    virtual void OnOverviewItemWindowDestroying(OverviewItem* overview_item,
                                                bool reposition) = 0;

   protected:
    virtual ~WindowDestructionDelegate() = default;
  };

  OverviewItem(aura::Window* window,
               OverviewSession* overview_session,
               OverviewGrid* overview_grid,
               WindowDestructionDelegate* destruction_delegate,
               EventHandlerDelegate* event_handler_delegate,
               bool eligible_for_shadow_config);

  OverviewItem(const OverviewItem&) = delete;
  OverviewItem& operator=(const OverviewItem&) = delete;

  ~OverviewItem() override;

  // May be null. Use `GetOrCreateOverviewItemView()` if a non-null return value
  // is needed.
  OverviewItemView* overview_item_view() { return overview_item_view_; }

  void set_eligible_for_shadow_config(bool eligible_for_shadow_config) {
    eligible_for_shadow_config_ = eligible_for_shadow_config;
  }

  // Closes window hosted by `this`.
  void CloseWindow();

  // Handles events forwarded from the contents view.
  void OnFocusedViewActivated();
  void OnFocusedViewClosed();

  // Updates the rounded corners on `this` only.
  void UpdateRoundedCorners();

  // Returns the `kTopViewInset` of the `transform_window_`.
  int GetTopInset() const;

  OverviewAnimationType GetExitOverviewAnimationType() const;
  OverviewAnimationType GetExitTransformAnimationType() const;

  // OverviewItemBase:
  void SetOpacity(float opacity) override;
  aura::Window::Windows GetWindowsForHomeGesture() override;
  void HideForSavedDeskLibrary(bool animate) override;
  void RevertHideForSavedDeskLibrary(bool animate) override;
  void UpdateMirrorsForDragging(bool is_touch_dragging) override;
  void DestroyMirrorsForDragging() override;
  aura::Window* GetWindow() override;
  std::vector<raw_ptr<aura::Window, VectorExperimental>> GetWindows() override;
  bool HasVisibleOnAllDesksWindow() override;
  bool Contains(const aura::Window* target) const override;
  OverviewItem* GetLeafItemForWindow(aura::Window* window) override;
  void RestoreWindow(bool reset_transform, bool animate) override;
  void SetBounds(const gfx::RectF& target_bounds,
                 OverviewAnimationType animation_type) override;
  gfx::Transform ComputeTargetTransform(
      const gfx::RectF& target_bounds) override;
  float GetItemScale(int height) override;
  void ScaleUpSelectedItem(OverviewAnimationType animation_type) override;
  void EnsureVisible() override;
  gfx::RectF GetWindowsUnionScreenBounds() const override;
  gfx::RectF GetTargetBoundsWithInsets() const override;
  gfx::RectF GetTransformedBounds() const override;
  std::vector<views::Widget*> GetFocusableWidgets() override;
  views::View* GetBackDropView() const override;
  bool ShouldHaveShadow() const override;
  void UpdateRoundedCornersAndShadow() override;
  float GetOpacity() const override;
  void PrepareForOverview() override;
  void SetShouldUseSpawnAnimation(bool value) override;
  void OnStartingAnimationComplete() override;
  void Restack() override;
  void StartDrag() override;
  void OnOverviewItemDragStarted() override;
  void OnOverviewItemDragEnded(bool snap) override;
  void OnOverviewItemContinuousScroll(const gfx::Transform& target_transform,
                                      float scroll_ratio) override;
  void UpdateCannotSnapWarningVisibility(bool animate) override;
  void HideCannotSnapWarning(bool animate) override;
  void OnMovingItemToAnotherDesk() override;
  void Shutdown() override;
  void AnimateAndCloseItem(bool up) override;
  void StopWidgetAnimation() override;
  OverviewItemFillMode GetOverviewItemFillMode() const override;
  void UpdateOverviewItemFillMode() override;
  const gfx::RoundedCornersF GetRoundedCorners() const override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  DragWindowController* window_mirror_for_dragging_for_testing() {
    return window_mirror_for_dragging_.get();
  }

 private:
  friend class OverviewTestBase;
  friend class ScopedOverviewTransformWindow;
  FRIEND_TEST_ALL_PREFIXES(OverviewSessionTest, DraggingOnMultipleDisplay);
  FRIEND_TEST_ALL_PREFIXES(SplitViewOverviewSessionTest, Clipping);

  // Creates `item_widget_` with `OverviewItemView` as its contents view.
  // `event_handler_delegate` specifies the concrete delegate to handle events,
  // which is `this` by default or the given `event_handler_delegate` if it's
  // not nullptr.
  void CreateItemWidget(EventHandlerDelegate* event_handler_delegate);

  // Functions to be called back when their associated animations complete.
  void OnWindowCloseAnimationCompleted();
  void OnItemSpawnedAnimationCompleted();
  void OnItemBoundsAnimationStarted();
  void OnItemBoundsAnimationEnded();

  // Returns the target that the window of `this` should be stacked below,
  // returns `nullptr` if no stacking is needed.
  aura::Window* GetStackBelowTarget() const;

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

  // Updates the `item_widget`'s bounds. Any change in bounds will be animated
  // from the current bounds to the new bounds as per the `animation_type`.
  void UpdateHeaderLayout(OverviewAnimationType animation_type);

  // Animates opacity of the |transform_window_| and its caption to |opacity|
  // using |animation_type|.
  void AnimateOpacity(float opacity, OverviewAnimationType animation_type);

  // Returns the type of animation to use for an item that manages a minimized
  // window.
  OverviewAnimationType GetExitOverviewAnimationTypeForMinimizedWindow(
      OverviewEnterExitType type);

  void CloseButtonPressed();

  // Creates the `OverviewItemView` and sets it as the widget's contents view.
  // This is a no-op if the `OverviewItemView` already exists.
  //
  // Use this if the item widget should definitely be visible at the callsite
  // (or will be very soon). Otherwise, use `overview_item_view()` and
  // gracefully handle if it's null.
  OverviewItemView& GetOrCreateOverviewItemView();

  // The root window this item is being displayed on.
  raw_ptr<aura::Window> root_window_;

  // The contained Window's wrapper.
  ScopedOverviewTransformWindow transform_window_;

  // The delegate to handle window destruction which is `OverviewGrid` for
  // single item or `OverviewGroupItem` for group item.
  const raw_ptr<WindowDestructionDelegate> window_destruction_delegate_;

  const raw_ptr<EventHandlerDelegate> event_handler_delegate_ = nullptr;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_ = false;

  // If true, `shadow_` is eligible to be created, false otherwise. The shadow
  // should not be created if `this` is hosted by an `OverviewGroupItem`
  // together with another `OverviewItem` (the group-level shadow will be
  // installed instead). However if a window inside an `OverviewGroupItem` is
  // destroyed, `eligible_for_shadow_config_` is set to true to ensure the
  // shadow bounds get updated correctly.
  bool eligible_for_shadow_config_;

  // The view associated with |item_widget_|. Contains a title, close button and
  // maybe a backdrop. Forwards certain events to |this|. May be null (see
  // `ScheduleOverviewItemViewInitialization()` for details).
  raw_ptr<OverviewItemView, DanglingUntriaged> overview_item_view_ = nullptr;

  // Responsible for mirrors that look like the window on all displays during
  // dragging.
  // TODO(sammiequon): We need two, one for the `item_widget_` and one for the
  // source window (if not minimized). If DragWindowController supports multiple
  // windows in the future, combine these.
  std::unique_ptr<DragWindowController> window_mirror_for_dragging_;

  // Disable animations on the contained window while it is being managed by the
  // overview item.
  wm::ScopedAnimationDisabler animation_disabler_;

  // Force `OverviewItem` to be visible while overview is in progress. This is
  // to ensure that overview items are properly marked as visible during all
  // parts of their animation (e.g. overview enter). This is only required
  // if those item's windows won't have snapshots.
  std::optional<aura::WindowOcclusionTracker::ScopedForceVisible>
      scoped_force_visible_;

  base::WeakPtrFactory<OverviewItem> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
