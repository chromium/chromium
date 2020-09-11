// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_
#define ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {
class KeyEvent;
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {
class OverviewDelegate;
class OverviewGrid;
class OverviewHighlightController;
class OverviewItem;
class OverviewWindowDragController;
class RoundedLabelWidget;

// The Overview shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT OverviewSession : public display::DisplayObserver,
                                   public aura::WindowObserver,
                                   public ui::EventHandler,
                                   public ShellObserver,
                                   public SplitViewObserver {
 public:
  // Callback which fills out the passed settings object. Used by several
  // functions so different callers can do similar animations with different
  // settings.
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;

  using WindowList = std::vector<aura::Window*>;

  explicit OverviewSession(OverviewDelegate* delegate);
  ~OverviewSession() override;

  // Initialize with the windows that can be selected.
  void Init(const WindowList& windows, const WindowList& hide_windows);

  // Perform cleanup that cannot be done in the destructor.
  void Shutdown();

  // Called when the last overview item from a grid is deleted.
  void OnGridEmpty();

  // Moves the current selection forwards or backwards.
  void IncrementSelection(bool forward);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Activates |item's| window.
  void SelectWindow(OverviewItem* item);

  // Sets the dragged window on the split view drag indicators.
  void SetSplitViewDragIndicatorsDraggedWindow(aura::Window* dragged_window);

  // If |state_on_root_window_being_dragged_in| is kNoDrag, this function sets
  // the state on every root window to kNoDrag. Otherwise it sets the state on
  // |root_window_being_dragged_in| to |state_on_root_window_being_dragged_in|,
  // and sets the state on other root windows to kOtherDisplay.
  void UpdateSplitViewDragIndicatorsWindowDraggingStates(
      const aura::Window* root_window_being_dragged_in,
      SplitViewDragIndicators::WindowDraggingState
          state_on_root_window_being_dragged_in);

  // Sets the state on every root window to kNoDrag.
  void ResetSplitViewDragIndicatorsWindowDraggingStates();

  // See |OverviewGrid::RearrangeDuringDrag|.
  void RearrangeDuringDrag(OverviewItem* dragged_item);

  // Updates the appearance of each drop target to visually indicate when the
  // dragged window is being dragged over it.
  void UpdateDropTargetsBackgroundVisibilities(
      OverviewItem* dragged_item,
      const gfx::PointF& location_in_screen);

  // Retrieves the window grid whose root window matches |root_window|. Returns
  // nullptr if the window grid is not found.
  OverviewGrid* GetGridWithRootWindow(aura::Window* root_window);

  // Adds |window| at the specified |index| into the grid with the same root
  // window. Does nothing if that grid does not exist in |grid_list_| or already
  // contains |window|. If |reposition| is true, repositions all items in the
  // target grid (unless it already contained |window|), except those in
  // |ignored_items|. If |animate| is true, animates the repositioning.
  // |animate| has no effect if |reposition| is false.
  void AddItem(aura::Window* window,
               bool reposition,
               bool animate,
               const base::flat_set<OverviewItem*>& ignored_items,
               size_t index);

  // Similar to the above function, but adds the window at the end of the grid.
  // This will use the spawn-item animation.
  // TODO(afakhry): Expose |use_spawn_animation| if needed.
  void AppendItem(aura::Window* window, bool reposition, bool animate);

  // Like |AddItem|, but adds |window| at the correct position according to MRU
  // order. If |reposition|, |animate|, and |restack| are all true, the stacking
  // order will be adjusted after the animation. If |restack| is true but at
  // least one of |reposition| and |animate| is false, the stacking order will
  // be adjusted immediately.
  void AddItemInMruOrder(aura::Window* window,
                         bool reposition,
                         bool animate,
                         bool restack);

  // Removes |overview_item| from the corresponding grid.
  void RemoveItem(OverviewItem* overview_item);
  void RemoveItem(OverviewItem* overview_item,
                  bool item_destroying,
                  bool reposition);

  void RemoveDropTargets();

  void InitiateDrag(OverviewItem* item,
                    const gfx::PointF& location_in_screen,
                    bool is_touch_dragging);
  void Drag(OverviewItem* item, const gfx::PointF& location_in_screen);
  void CompleteDrag(OverviewItem* item, const gfx::PointF& location_in_screen);
  void StartNormalDragMode(const gfx::PointF& location_in_screen);
  void Fling(OverviewItem* item,
             const gfx::PointF& location_in_screen,
             float velocity_x,
             float velocity_y);
  void ActivateDraggedWindow();
  void ResetDraggedWindowGesture();

  // Called when a window (either it's browser window or an app window)
  // start/continue/end being dragged in tablet mode by swiping from the top
  // of the screen to drag from top or by swiping from the shelf to drag from
  // bottom .
  // TODO(xdai): Currently it doesn't work for multi-display scenario.
  void OnWindowDragStarted(aura::Window* dragged_window, bool animate);
  void OnWindowDragContinued(
      aura::Window* dragged_window,
      const gfx::PointF& location_in_screen,
      SplitViewDragIndicators::WindowDraggingState window_dragging_state);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         const gfx::PointF& location_in_screen,
                         bool should_drop_window_into_overview,
                         bool snap);
  // Shows or Hides all windows (including drop target window & desk widget) in
  // overview. It's used when dragging a window from bottom, when the user slows
  // down or stops dragging the window, shows overview windows and when the user
  // resumes dragging, hides overview windows.
  void SetVisibleDuringWindowDragging(bool visible, bool animate);

  // Positions all overview items except those in |ignored_items|.
  void PositionWindows(bool animate,
                       const base::flat_set<OverviewItem*>& ignored_items = {});

  // Returns true if |window| is currently showing in overview.
  bool IsWindowInOverview(const aura::Window* window);

  // Returns the overview item for |window|, or nullptr if |window| doesn't have
  // a corresponding item in overview mode.
  OverviewItem* GetOverviewItemForWindow(const aura::Window* window);

  // Set the window grid that's displaying in |root_window| not animate when
  // exiting overview mode, i.e., all window items in the grid will not animate
  // when exiting overview mode. It may be called in two cases: 1) When a window
  // gets snapped (either from overview or not) and thus cause the end of the
  // overview mode, we should not do the exiting animation; 2) When a window
  // is dragged around and when released, it causes the end of the overview
  // mode, we also should not do the exiting animation.
  void SetWindowListNotAnimatedWhenExiting(aura::Window* root_window);

  // Shifts and fades the grid in |grid_list_| associated with |location|.
  // Returns a ui::ScopedLayerAnimationSettings object for the caller to
  // observe.
  std::unique_ptr<ui::ScopedLayerAnimationSettings>
  UpdateGridAtLocationYPositionAndOpacity(
      int64_t display_id,
      float new_y,
      float opacity,
      UpdateAnimationSettingsCallback callback);

  // Updates all the overview items' mask and shadow.
  void UpdateRoundedCornersAndShadow();

  // Called when the overview mode starting animation completes. |canceled| is
  // true when the starting animation is interrupted by ending overview mode. If
  // |canceled| is false and |should_focus_overview| is true, then
  // |overview_focus_widget_| shall gain focus. |should_focus_overview| has no
  // effect when |canceled| is true.
  void OnStartingAnimationComplete(bool canceled, bool should_focus_overview);

  // Called when windows are being activated/deactivated during
  // overview mode.
  void OnWindowActivating(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active);

  // Gets the window which keeps focus for the duration of overview mode.
  aura::Window* GetOverviewFocusWindow();

  // Returns the window highlighted by the selector widget.
  aura::Window* GetHighlightedWindow();

  // Suspends/Resumes window re-positiong in overview.
  void SuspendReposition();
  void ResumeReposition();

  // Returns true if all its window grids don't have any window item.
  bool IsEmpty() const;

  // If |focus| is true, restores focus to |restore_focus_window_|. Sets
  // |restore_focus_window_| to null regardless of |focus|.
  void ResetFocusRestoreWindow(bool focus);

  // Handles requests to active or close the currently highlighted |item|.
  void OnHighlightedItemActivated(OverviewItem* item);
  void OnHighlightedItemClosed(OverviewItem* item);

  // Called explicitly (with no list of observers) by the |RootWindowController|
  // of |root|, so that the associated grid is properly removed and destroyed.
  // Note: Usually, when a display is removed, it causes a window activation
  // which ends overview mode, and then this function does not get called. This
  // function is only needed for when overview mode cannot be ended (see
  // |OverviewController::CanEndOverview| and https://crbug.com/1024325).
  void OnRootWindowClosing(aura::Window* root);

  // Returns the current dragged overview item if any. Note that windows that
  // are dragged into overview from the shelf don't have an OverviewItem while
  // dragging.
  OverviewItem* GetCurrentDraggedOverviewItem() const;

  // Overview objects which handle events (OverviewItemView,
  // OverviewGridEventHandler) should call this function to check if they can
  // process an event. Returns false if an overview item other than |sender|
  // (which may be nullptr in the case of events on the wallpaper) is already
  // being dragged, or if a window is currently being dragged from the bottom.
  // This is so we can allow switching finger while dragging, but not allow
  // dragging two or more items. The first |CanProcessEvent()| calls the second
  // with |sender| as nullptr (i.e. event processed by
  // OverviewGridEventHandler). When |sender| is nullptr, |from_touch_gesture|
  // does not matter.
  bool CanProcessEvent() const;
  bool CanProcessEvent(OverviewItem* sender, bool from_touch_gesture) const;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ShelObserver:
  void OnShellDestroying() override;
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

  OverviewDelegate* delegate() { return delegate_; }

  bool is_shutting_down() const { return is_shutting_down_; }
  void set_is_shutting_down(bool is_shutting_down) {
    is_shutting_down_ = is_shutting_down;
  }

  const std::vector<std::unique_ptr<OverviewGrid>>& grid_list() const {
    return grid_list_;
  }

  size_t num_items() const { return num_items_; }

  OverviewEnterExitType enter_exit_overview_type() const {
    return enter_exit_overview_type_;
  }
  void set_enter_exit_overview_type(OverviewEnterExitType val) {
    enter_exit_overview_type_ = val;
  }

  OverviewWindowDragController* window_drag_controller() {
    return window_drag_controller_.get();
  }

  OverviewHighlightController* highlight_controller() {
    return highlight_controller_.get();
  }

  RoundedLabelWidget* no_windows_widget_for_testing() {
    return no_windows_widget_.get();
  }

 private:
  friend class DesksAcceleratorsTest;
  friend class OverviewSessionTest;
  class AccessibilityFocusAnnotator;

  // Helper function that moves the highlight forward or backward on the
  // corresponding window grid.
  void Move(bool reverse);

  // Helper function that processes a key event and maybe scrolls the overview
  // grid on the primary display.
  bool ProcessForScrolling(const ui::KeyEvent& event);

  // Removes all observers that were registered during construction and/or
  // initialization.
  void RemoveAllObservers();

  void UpdateNoWindowsWidget();

  void RefreshNoWindowsWidgetBounds(bool animate);

  void OnItemAdded(aura::Window* window);

  // Tracks observed windows.
  base::flat_set<aura::Window*> observed_windows_;

  // Weak pointer to the overview delegate which will be called when a selection
  // is made.
  OverviewDelegate* delegate_;

  // A weak pointer to the window which was focused on starting overview. If
  // overview is canceled the focus should be restored to this window.
  aura::Window* restore_focus_window_ = nullptr;

  // A hidden window that receives focus while in overview mode. It is needed
  // because accessibility needs something focused for it to work and we cannot
  // use one of the overview windows otherwise wm::ActivateWindow will not
  // work.
  // TODO(sammiequon): Focus the grid desks widget if it is always available, or
  // we may be able to add some mechanism to trigger accessibility events
  // without a focused window.
  std::unique_ptr<views::Widget> overview_focus_widget_;

  // A widget that is shown if we entered overview without any windows opened.
  std::unique_ptr<RoundedLabelWidget> no_windows_widget_;

  // True when performing operations that may cause window activations. This is
  // used to prevent handling the resulting expected activation. This is
  // initially true until this is initialized.
  bool ignore_activations_ = true;

  // True when overview mode is exiting.
  bool is_shutting_down_ = false;

  // List of all the window overview grids, one for each root window.
  std::vector<std::unique_ptr<OverviewGrid>> grid_list_;

  // The following variables are used for metric collection purposes. All of
  // them refer to this particular overview session and are not cumulative:
  // The time when overview was started.
  base::Time overview_start_time_;

  // The number of arrow key presses.
  size_t num_key_presses_ = 0;

  // The number of items in the overview.
  size_t num_items_ = 0;

  // True if we are currently using keyboard (control + left/right) to scroll
  // through the grid.
  bool is_keyboard_scrolling_grid_ = false;

  // Stores the overview enter/exit type. See the enum declaration for
  // information on how these types affect overview mode.
  OverviewEnterExitType enter_exit_overview_type_ =
      OverviewEnterExitType::kNormal;

  // The selected item when exiting overview mode. nullptr if no window
  // selected.
  OverviewItem* selected_item_ = nullptr;

  // The drag controller for a window in the overview mode.
  std::unique_ptr<OverviewWindowDragController> window_drag_controller_;

  std::unique_ptr<ScopedOverviewHideWindows> hide_overview_windows_;

  std::unique_ptr<OverviewHighlightController> highlight_controller_;

  // Updates accessibility with the correct focus order among all overview
  // widgets.
  std::unique_ptr<AccessibilityFocusAnnotator> accessibility_focus_annotator_;

  DISALLOW_COPY_AND_ASSIGN(OverviewSession);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_
