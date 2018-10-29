// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/scoped_hide_overview_windows.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/wm/public/activation_change_observer.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace views {
class Textfield;
class Widget;
}  // namespace views

namespace ash {

class OverviewWindowDragController;
class SplitViewDragIndicators;
class WindowGrid;
class WindowSelectorDelegate;
class WindowSelectorItem;
class WindowSelectorTest;

enum class IndicatorState;

// The WindowSelector shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT WindowSelector : public display::DisplayObserver,
                                  public aura::WindowObserver,
                                  public ::wm::ActivationChangeObserver,
                                  public views::TextfieldController,
                                  public SplitViewController::Observer {
 public:
  // Returns true if the window can be selected in overview mode.
  static bool IsSelectable(const aura::Window* window);

  enum Direction { LEFT, UP, RIGHT, DOWN };

  enum class OverviewTransition {
    kEnter,       // In the entering process of overview.
    kInOverview,  // Already in overview.
    kExit         // In the exiting process of overview.
  };

  // Enum describing the different ways overview can be entered or exited.
  enum class EnterExitOverviewType {
    // The default way, window(s) animate from their initial bounds to the grid
    // bounds. Window(s) that are not visible to the user do not get animated.
    // This should always be the type when in clamshell mode.
    kNormal,
    // When going to or from a state which all window(s) are minimized, slides
    // the windows in or out. This will minimize windows on exit if needed, so
    // that we do not need to add a delayed observer to handle minimizing the
    // windows after overview exit animations are finished.
    kWindowsMinimized,
    // Overview can be closed by swiping up from the shelf. In this mode, the
    // call site will handle shifting the bounds of the windows, so overview
    // code does not need to handle any animations. This is an exit only type.
    kSwipeFromShelf,
    // Overview can be opened by start dragging a window from top or be closed
    // if the dragged window restores back to maximized/full-screened. On enter
    // this mode is same as kNormal, except when all windows are minimized, the
    // launcher does not animate in. On exit this mode is used to avoid the
    // update bounds animation of the windows in overview grid on overview mode
    // ended.
    kWindowDragged
  };

  // Callback which fills out the passed settings object. Used by several
  // functions so different callers can do similar animations with different
  // settings.
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings,
                                   bool observe)>;

  using WindowList = std::vector<aura::Window*>;

  explicit WindowSelector(WindowSelectorDelegate* delegate);
  ~WindowSelector() override;

  // Initialize with the windows that can be selected.
  void Init(const WindowList& windows, const WindowList& hide_windows);

  // Perform cleanup that cannot be done in the destructor.
  void Shutdown();

  // Cancels window selection.
  void CancelSelection();

  // Called when the last window selector item from a grid is deleted.
  void OnGridEmpty(WindowGrid* grid);

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Activates |item's| window.
  void SelectWindow(WindowSelectorItem* item);

  // Called to set bounds for window grids. Used for split view.
  void SetBoundsForWindowGridsInScreenIgnoringWindow(
      const gfx::Rect& bounds,
      WindowSelectorItem* ignored_item);

  // Called to show or hide the split view drag indicators. This will do
  // nothing if split view is not enabled. |event_location| is used to reparent
  // |split_view_drag_indicators_|'s widget, if necessary.
  void SetSplitViewDragIndicatorsIndicatorState(
      IndicatorState indicator_state,
      const gfx::Point& event_location);

  // Retrieves the window grid whose root window matches |root_window|. Returns
  // nullptr if the window grid is not found.
  WindowGrid* GetGridWithRootWindow(aura::Window* root_window);

  // Add |window| to the grid in |grid_list_| with the same root window. Does
  // nothing if the grid already contains |window|. And if |reposition| is true,
  // re-position all windows in the target window grid. If |animate| is true,
  // re-position with animation. This function may be called in two scenarios:
  // 1) when a item in split view mode was previously snapped but should now be
  // returned to the window grid (e.g. split view divider dragged to either
  // edge, or a window is snapped to a postion that already has a snapped
  // window); 2) when a window (not from overview) is dragged while overview is
  // open and the window is dropped on the drop target, the dragged window is
  // then added to the overview.
  void AddItem(aura::Window* window, bool reposition, bool animate);

  // Removes the window selector item from the overview window grid. And if
  // |reposition| is true, re-position all windows in the target window grid.
  // This may be called in two scenarioes: 1) when a user drags an overview item
  // to snap to one side of the screen, the item should be removed from the
  // overview grid; 2) when a window (not from overview) ends its dragging while
  // overview is open, the drop target should be removed. Note in both cases,
  // the windows in the window grid do not need to be repositioned.
  void RemoveWindowSelectorItem(WindowSelectorItem* item, bool reposition);

  void InitiateDrag(WindowSelectorItem* item,
                    const gfx::Point& location_in_screen);
  void Drag(WindowSelectorItem* item, const gfx::Point& location_in_screen);
  void CompleteDrag(WindowSelectorItem* item,
                    const gfx::Point& location_in_screen);
  void StartSplitViewDragMode(const gfx::Point& location_in_screen);
  void Fling(WindowSelectorItem* item,
             const gfx::Point& location_in_screen,
             float velocity_x,
             float velocity_y);
  void ActivateDraggedWindow();
  void ResetDraggedWindowGesture();

  // Called when a window (either it's browser window or an app window)
  // start/continue/end being dragged in tablet mode.
  // TODO(xdai): Currently it doesn't work for multi-display scenario.
  void OnWindowDragStarted(aura::Window* dragged_window, bool animate);
  void OnWindowDragContinued(aura::Window* dragged_window,
                             const gfx::Point& location_in_screen,
                             IndicatorState indicator_state);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         const gfx::Point& location_in_screen,
                         bool should_drop_window_into_overview);

  // Positions all of the windows in the overview, except |ignored_item|.
  void PositionWindows(bool animate,
                       WindowSelectorItem* ignored_item = nullptr);

  // If we are in middle of ending overview mode.
  bool IsShuttingDown() const;

  // Checks if the grid associated with a given |root_window| needs to have the
  // wallpaper animated. Returns false if one of the grids windows covers the
  // the entire workspace, true otherwise.
  bool ShouldAnimateWallpaper(aura::Window* root_window);

  // Returns true if |window| is currently showing in overview.
  bool IsWindowInOverview(const aura::Window* window);

  // Set the window grid that's displaying in |root_window| not animate when
  // exiting overview mode, i.e., all window items in the grid will not animate
  // when exiting overview mode. It may be called in two cases: 1) When a window
  // gets snapped (either from overview or not) and thus cause the end of the
  // overview mode, we should not do the exiting animation; 2) When a window
  // is dragged around and when released, it causes the end of the overview
  // mode, we also should not do the exiting animation.
  void SetWindowListNotAnimatedWhenExiting(aura::Window* root_window);

  // Shifts and fades the grid in |grid_list_| associated with |location|.
  void UpdateGridAtLocationYPositionAndOpacity(
      int64_t display_id,
      int new_y,
      float opacity,
      const gfx::Rect& work_area,
      UpdateAnimationSettingsCallback callback);

  // Shows or hides all the window selector items' mask and shadow.
  void UpdateMaskAndShadow(bool show);

  WindowSelectorDelegate* delegate() { return delegate_; }

  SplitViewDragIndicators* split_view_drag_indicators() {
    return split_view_drag_indicators_.get();
  }

  int text_filter_bottom() const { return text_filter_bottom_; }

  EnterExitOverviewType enter_exit_overview_type() const {
    return enter_exit_overview_type_;
  }
  void set_enter_exit_overview_type(EnterExitOverviewType val) {
    enter_exit_overview_type_ = val;
  }

  OverviewWindowDragController* window_drag_controller() {
    return window_drag_controller_.get();
  }

  const std::vector<std::unique_ptr<WindowGrid>>& grid_list_for_testing()
      const {
    return grid_list_;
  }

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // SplitViewController::Observer:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

 private:
  friend class WindowSelectorTest;

  // Returns the aura::Window for |text_filter_widget_|.
  aura::Window* GetTextFilterWidgetWindow();

  // Repositions and resizes |text_filter_widget_| on
  // DisplayMetricsChanged event.
  void RepositionTextFilterOnDisplayMetricsChange();

  // |focus|, restores focus to the stored window.
  void ResetFocusRestoreWindow(bool focus);

  // Helper function that moves the selection widget to |direction| on the
  // corresponding window grid.
  void Move(Direction direction, bool animate);

  // Removes all observers that were registered during construction and/or
  // initialization.
  void RemoveAllObservers();

  // Called when the display area for the overview window grids changed.
  void OnDisplayBoundsChanged();

  // Returns true if all its window grids don't have any window item.
  bool IsEmpty();

  // Tracks observed windows.
  std::set<aura::Window*> observed_windows_;

  // Weak pointer to the selector delegate which will be called when a
  // selection is made.
  WindowSelectorDelegate* delegate_;

  // A weak pointer to the window which was focused on beginning window
  // selection. If window selection is canceled the focus should be restored to
  // this window.
  aura::Window* restore_focus_window_;

  // True when performing operations that may cause window activations. This is
  // used to prevent handling the resulting expected activation.
  bool ignore_activations_ = false;

  // List of all the window overview grids, one for each root window.
  std::vector<std::unique_ptr<WindowGrid>> grid_list_;

  // The owner of the widget which displays splitview related information in
  // overview mode. This will be nullptr if split view is not enabled.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  // Tracks the index of the root window the selection widget is in.
  size_t selected_grid_index_ = 0;

  // The following variables are used for metric collection purposes. All of
  // them refer to this particular overview session and are not cumulative:
  // The time when overview was started.
  base::Time overview_start_time_;

  // The number of arrow key presses.
  size_t num_key_presses_ = 0;

  // The number of items in the overview.
  size_t num_items_ = 0;

  // Indicates if the text filter is shown on screen (rather than above it).
  bool showing_text_filter_ = false;

  // Window text filter widget. As the user writes on it, we filter the items
  // in the overview. It is also responsible for handling overview key events,
  // such as enter key to select.
  std::unique_ptr<views::Widget> text_filter_widget_;

  // The current length of the string entered into the text filtering textfield.
  size_t text_filter_string_length_ = 0;

  // The number of times the text filtering textfield has been cleared of text
  // during this overview mode session.
  size_t num_times_textfield_cleared_ = 0;

  // The distance between the top edge of the screen and the bottom edge of
  // the text filtering textfield.
  int text_filter_bottom_ = 0;

  // Stores the overview enter/exit type. See the enum declaration for
  // information on how these types affect overview mode.
  EnterExitOverviewType enter_exit_overview_type_ =
      EnterExitOverviewType::kNormal;

  // The selected item when exiting overview mode. nullptr if no window
  // selected.
  WindowSelectorItem* selected_item_ = nullptr;

  // The drag controller for a window in the overview mode.
  std::unique_ptr<OverviewWindowDragController> window_drag_controller_;

  std::unique_ptr<ScopedHideOverviewWindows> hide_overview_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
