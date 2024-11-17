// Copyright 2013 The Chromium Authors
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
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_focus_cycler.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/snap_group/snap_group_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {
class KeyEvent;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

class BirchBarController;
class OverviewDelegate;
class OverviewGrid;
class OverviewItem;
class OverviewItemBase;
class OverviewWindowDragController;
class SavedDeskDialogController;
class SavedDeskPresenter;
class ScopedFloatContainerStacker;
class WindowOcclusionCalculator;

// The Overview shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT OverviewSession : public display::DisplayObserver,
                                   public aura::WindowObserver,
                                   public ui::EventHandler,
                                   public ShellObserver,
                                   public SplitViewObserver,
                                   public DesksController::Observer,
                                   public SnapGroupObserver {
 public:
  explicit OverviewSession(OverviewDelegate* delegate);

  OverviewSession(const OverviewSession&) = delete;
  OverviewSession& operator=(const OverviewSession&) = delete;

  ~OverviewSession() override;

  // Initialize with the windows that can be selected.
  void Init(
      const aura::Window::Windows& windows,
      const aura::Window::Windows& hide_windows,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);

  // Perform cleanup that cannot be done in the destructor.
  void Shutdown();

  // Called when the last overview item from a grid is deleted.
  void OnGridEmpty();

  // Moves the current selection forwards or backwards.
  void IncrementSelection(bool forward);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Activates the window associated with the `item`.
  void SelectWindow(OverviewItemBase* item);

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
  void RearrangeDuringDrag(OverviewItemBase* dragged_item);

  // Updates the appearance of each drop target to visually indicate when the
  // dragged window is being dragged over it.
  void UpdateDropTargetsBackgroundVisibilities(
      OverviewItemBase* dragged_item,
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
               const base::flat_set<OverviewItemBase*>& ignored_items,
               size_t index);

  // Similar to the above function, but adds the window at the end of the grid.
  // This will use the spawn-item animation.
  void AppendItem(aura::Window* window, bool reposition, bool animate);

  // Like |AddItem|, but adds |window| at the correct position according to MRU
  // order. If |reposition|, |animate|, and |restack| are all true, the stacking
  // order will be adjusted after the animation. If |restack| is true but at
  // least one of |reposition| and |animate| is false, the stacking order will
  // be adjusted immediately.
  void AddItemInMruOrder(aura::Window* window,
                         bool reposition,
                         bool animate,
                         bool restack,
                         bool use_spawn_animation);

  // Removes |overview_item| from the corresponding grid.
  void RemoveItem(OverviewItemBase* overview_item);
  void RemoveItem(OverviewItemBase* overview_item,
                  bool item_destroying,
                  bool reposition);

  void RemoveDropTargets();

  void InitiateDrag(OverviewItemBase* item,
                    const gfx::PointF& location_in_screen,
                    bool is_touch_dragging,
                    OverviewItemBase* event_source_item);
  void Drag(OverviewItemBase* item, const gfx::PointF& location_in_screen);
  void CompleteDrag(OverviewItemBase* item,
                    const gfx::PointF& location_in_screen);
  void StartNormalDragMode(const gfx::PointF& location_in_screen);
  void Fling(OverviewItemBase* item,
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

  // This is called on drag end for WebUI Tab Strip similar to
  // OnWindowDragEnded. Since WebUI tab strip tab dragging only creates new
  // window on drag end, both OnWindowDragStarted and OnWindowDragContinued are
  // not being called.
  void MergeWindowIntoOverviewForWebUITabStrip(aura::Window* dragged_window);

  // Positions all overview items except those in |ignored_items|.
  void PositionWindows(
      bool animate,
      const base::flat_set<OverviewItemBase*>& ignored_items = {});

  // Returns true if |window| is currently showing in overview.
  bool IsWindowInOverview(const aura::Window* window);

  // Returns the `OverviewItemBase` for the given `window`, or nullptr if
  // `window` doesn't have a corresponding item in overview mode.
  OverviewItemBase* GetOverviewItemForWindow(const aura::Window* window);

  // Set the window grid that's displaying in |root_window| not animate when
  // exiting overview mode, i.e., all window items in the grid will not animate
  // when exiting overview mode. It may be called in two cases: 1) When a window
  // gets snapped (either from overview or not) and thus cause the end of the
  // overview mode, we should not do the exiting animation; 2) When a window
  // is dragged around and when released, it causes the end of the overview
  // mode, we also should not do the exiting animation.
  void SetWindowListNotAnimatedWhenExiting(aura::Window* root_window);

  // Shifts and fades the grid in |grid_list_| associated with |location|.

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
  void OnWindowActivating(wm::ActivationChangeObserver::ActivationReason reason,
                          aura::Window* gained_active,
                          aura::Window* lost_active);

  // Returns true when either the `SavedDeskLibraryView` or
  // `SavedDeskDialog` is the window that is losing activation.
  bool IsSavedDeskUiLosingActivation(aura::Window* lost_active);

  // Gets the window which keeps focus for the duration of overview mode.
  aura::Window* GetOverviewFocusWindow() const;

  // Returns the window associated with the focused item. Returns null if no
  // item has focus (i.e. desk mini view is focused, or nothing is focused).
  aura::Window* GetFocusedWindow();

  // Suspends/Resumes window re-positiong in overview.
  void SuspendReposition();
  void ResumeReposition();

  // Returns true if all its window grids don't have any window item.
  bool IsEmpty() const;

  // If |restore| is true, activate window |active_window_before_overview_|.
  // This is usually called when exiting overview to restore window activation
  // to the window that was active before entering overview. If |restore| is
  // false, reset |active_window_before_overview_| to nullptr so that window
  // activation will not be restore when overview is ended.
  void RestoreWindowActivation(bool restore);

  // Handles requests to active or close the currently focused `item`.
  void OnFocusedItemActivated(OverviewItem* item);
  void OnFocusedItemClosed(OverviewItem* item);

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
  OverviewItemBase* GetCurrentDraggedOverviewItem() const;

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
  bool CanProcessEvent(OverviewItemBase* sender, bool from_touch_gesture) const;

  // Returns true if |window| is not nullptr and equals
  // |active_window_before_overview_|.
  bool IsWindowActiveWindowBeforeOverview(aura::Window* window) const;

  // Used when feature ContinuousOverviewScrollAnimation is enabled. If a
  // continuous scroll is in progress, position windows and desk bar
  // continuously based on the y-distance of the scroll. If the scroll is
  // ending, animate windows and desk bar to their final positions.
  bool HandleContinuousScrollIntoOverview(float y_offset);

  // Shows the saved desk library. Creates the widget if needed. The desks bar
  // will be expanded if it isn't already. Focuses the item which matches
  // `item_to_focus` on the display associated with `root_window`.
  void ShowSavedDeskLibrary(const base::Uuid& item_to_focus,
                            const std::u16string& saved_desk_name,
                            aura::Window* const root_window);

  // Hides the saved desk library and reshows the overview items. Updates the
  // save desk button if we are not exiting overview.
  void HideSavedDeskLibrary();

  // True if the saved desk library is shown.
  bool IsShowingSavedDeskLibrary() const;

  // True if we want to enter overview without animations.
  bool ShouldEnterWithoutAnimations() const;

  // Updates the focusable overview widgets so that they point to the correct
  // next and previous widgets for a11y purposes. Needs to be updated when a
  // piece of UI is shown or hidden.
  void UpdateAccessibilityFocus();

  void UpdateFrameThrottling();

  base::WeakPtr<OverviewSession> GetWeakPtr();

  // DesksController::Observer:
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowAdded(aura::Window* new_window) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // ShellObserver:
  void OnShellDestroying() override;
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;
  void OnUserWorkAreaInsetsChanged(aura::Window* root_window) override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

  // SnapGroupObserver:
  void OnSnapGroupRemoving(SnapGroup* snap_group,
                           SnapGroupExitPoint exit_pint) override;

  OverviewDelegate* delegate() { return delegate_; }

  views::Widget* overview_focus_widget() {
    return overview_focus_widget_.get();
  }

  bool ignore_activations() const { return ignore_activations_; }
  void set_ignore_activations(bool ignore_activations) {
    ignore_activations_ = ignore_activations;
  }

  bool is_shutting_down() const { return is_shutting_down_; }
  void set_is_shutting_down(bool is_shutting_down) {
    is_shutting_down_ = is_shutting_down;
  }

  const std::vector<std::unique_ptr<OverviewGrid>>& grid_list() const {
    return grid_list_;
  }

  OverviewEnterExitType enter_exit_overview_type() const {
    return enter_exit_overview_type_;
  }

  void set_enter_exit_overview_type(OverviewEnterExitType val) {
    enter_exit_overview_type_ = val;
  }

  OverviewEndAction overview_end_action() const { return overview_end_action_; }

  void set_overview_end_action(OverviewEndAction overview_end_action) {
    overview_end_action_ = overview_end_action;
  }

  OverviewWindowDragController* window_drag_controller() {
    return window_drag_controller_.get();
  }

  ScopedOverviewHideWindows* hide_windows_for_saved_desks_grid() {
    return hide_windows_for_saved_desks_grid_.get();
  }

  OverviewFocusCycler* focus_cycler() { return &focus_cycler_; }

  SavedDeskPresenter* saved_desk_presenter() {
    return saved_desk_presenter_.get();
  }

  SavedDeskDialogController* saved_desk_dialog_controller() {
    return saved_desk_dialog_controller_.get();
  }

  ScopedFloatContainerStacker* float_container_stacker() {
    return float_container_stacker_.get();
  }

  BirchBarController* birch_bar_controller() {
    return birch_bar_controller_.get();
  }

  void set_auto_add_windows_enabled(bool enabled) {
    auto_add_windows_enabled_ = enabled;
  }

  void set_allow_empty_desk_without_exiting(bool enabled) {
    allow_empty_desk_without_exiting_ = enabled;
  }

 private:
  friend class DesksAcceleratorsTest;
  friend class OverviewTestBase;
  friend class TestOverviewItemsOnOverviewModeEndObserver;
  FRIEND_TEST_ALL_PREFIXES(SplitViewControllerTest,
                           ItemsRemovedFromOverviewOnSnap);

  // Called when tablet mode changes.
  void OnTabletModeChanged();

  // Helper function that moves the focus ring forward or backward on the
  // corresponding window grid.
  void Move(bool reverse);

  // Helper function that processes a key event and maybe scrolls the overview
  // grid on the primary display.
  bool ProcessForScrolling(const ui::KeyEvent& event);

  // Removes all observers that were registered during construction and/or
  // initialization.
  void RemoveAllObservers();

  // Updates the no windows widget on each `OverviewGrid`.
  void UpdateNoWindowsWidgetOnEachGrid(bool animate, bool is_continuous_enter);

  void OnItemAdded(aura::Window* window);

  size_t GetNumWindows() const;

  // Let `SplitViewOverviewSession` handle the `event` if it is alive.
  void MaybeDelegateEventToSplitViewOverviewSession(ui::LocatedEvent* event);

  // Weak pointer to the overview delegate which will be called when a selection
  // is made.
  raw_ptr<OverviewDelegate, DanglingUntriaged> delegate_;

  // A weak pointer to the window which was active on starting overview. If
  // overview is canceled the activation should be restored to this window.
  raw_ptr<aura::Window> active_window_before_overview_ = nullptr;

  // A hidden window that receives focus while in overview mode. It is needed
  // because accessibility needs something focused for it to work and we cannot
  // use one of the overview windows otherwise wm::ActivateWindow will not
  // work.
  // TODO(sammiequon): Focus the grid desks widget if it is always available, or
  // we may be able to add some mechanism to trigger accessibility events
  // without a focused window.
  std::unique_ptr<views::Widget> overview_focus_widget_;

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
  // The number of arrow and tab key presses.
  size_t num_key_presses_ = 0;

  // The number of windows in overview when it was started.
  size_t num_start_windows_ = 0;

  // True if we are currently using keyboard (control + left/right) to scroll
  // through the grid.
  bool is_keyboard_scrolling_grid_ = false;

  // Stores the overview enter/exit type. See the enum declaration for
  // information on how these types affect overview mode.
  OverviewEnterExitType enter_exit_overview_type_ =
      OverviewEnterExitType::kNormal;

  // Stores the action that ends the overview mode.
  OverviewEndAction overview_end_action_ = OverviewEndAction::kMaxValue;

  // The selected item when exiting overview mode. nullptr if no window
  // selected.
  raw_ptr<OverviewItemBase, DanglingUntriaged> selected_item_ = nullptr;

  // The drag controller for a window in the overview mode.
  std::unique_ptr<OverviewWindowDragController> window_drag_controller_;

  std::unique_ptr<ScopedOverviewHideWindows> hide_overview_windows_;

  // Scoped windows to hide for saved desks grid. For now, this contains the
  // overview item window and its corresponding real window to make sure such
  // windows are not shown via other events for saved desks grid.
  std::unique_ptr<ScopedOverviewHideWindows> hide_windows_for_saved_desks_grid_;

  OverviewFocusCycler focus_cycler_{this};

  // The object responsible to talking to the desk model.
  std::unique_ptr<SavedDeskPresenter> saved_desk_presenter_;

  // Controls showing and hiding dialogs associated with the saved desks
  // feature.
  std::unique_ptr<SavedDeskDialogController> saved_desk_dialog_controller_;

  // Scoped object which handles stacking the float container while inside
  // overview so it can appear under regular windows during several operations,
  // such as scrolling and dragging.
  std::unique_ptr<ScopedFloatContainerStacker> float_container_stacker_;

  // The controller to manage the birch bars.
  std::unique_ptr<BirchBarController> birch_bar_controller_;

  // Boolean to indicate whether chromeVox is enabled or not.
  bool chromevox_enabled_;

  // When non-null, windows changes on this desk are observed.
  raw_ptr<const Desk, DanglingUntriaged> observing_desk_ = nullptr;

  // This is true *while* an overview item is being dynamically added. It is
  // used to avoid recursively adding overview items.
  bool is_adding_new_item_ = false;

  // When true, windows added to the observed desk are automatically added to
  // the overview session.
  bool auto_add_windows_enabled_ = true;

  // When true, the overview session is not exited when the last window is
  // removed.
  bool allow_empty_desk_without_exiting_ = false;

  std::optional<display::ScopedDisplayObserver> display_observer_;

  base::ScopedObservation<DesksController, DesksController::Observer>
      desks_controller_observation_{this};

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      active_window_before_overview_observation_{this};
  base::WeakPtrFactory<OverviewSession> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_
