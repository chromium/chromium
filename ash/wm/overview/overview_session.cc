// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_settings.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_label_widget.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Values for the no items indicator which appears when opening overview mode
// with no opened windows.
constexpr int kNoItemsIndicatorHeightDp = 32;
constexpr int kNoItemsIndicatorHorizontalPaddingDp = 16;
constexpr int kNoItemsIndicatorRoundingDp = 16;
constexpr int kNoItemsIndicatorVerticalPaddingDp = 8;
constexpr SkColor kNoItemsIndicatorBackgroundColor =
    SkColorSetA(SK_ColorBLACK, 204);
constexpr SkColor kNoItemsIndicatorTextColor = SK_ColorWHITE;

// Values for scrolling the grid by using the keyboard.
// TODO(sammiequon): See if we can use the same values used for web scrolling.
constexpr int kKeyboardPressScrollingDp = 75;
constexpr int kKeyboardHoldScrollingDp = 15;

void EndOverview() {
  Shell::Get()->overview_controller()->EndOverview();
}

}  // namespace

OverviewSession::OverviewSession(OverviewDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(window_util::GetFocusedWindow()),
      overview_start_time_(base::Time::Now()),
      highlight_controller_(
          std::make_unique<OverviewHighlightController>(this)) {
  DCHECK(delegate_);
  Shell::Get()->AddPreTargetHandler(this);
}

OverviewSession::~OverviewSession() {
  DCHECK(observed_windows_.empty());
  // Don't delete |window_drag_controller_| yet since the stack might be still
  // using it.
  if (window_drag_controller_) {
    window_drag_controller_->ResetOverviewSession();
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, window_drag_controller_.release());
  }
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, to make indirect method calls on a partially
// constructed object.
void OverviewSession::Init(const WindowList& windows,
                           const WindowList& hide_windows) {
  Shell::Get()->AddShellObserver(this);

  hide_overview_windows_ = std::make_unique<ScopedOverviewHideWindows>(
      std::move(hide_windows), /*force_hidden=*/false);
  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const aura::Window* a, const aura::Window* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (auto* root : root_windows) {
    // Observed switchable containers for newly created windows on all root
    // windows.
    for (auto* container :
         GetSwitchableContainersForRoot(root, /*active_desk_only=*/true)) {
      container->AddObserver(this);
      observed_windows_.insert(container);
    }

    auto grid = std::make_unique<OverviewGrid>(root, windows, this);
    num_items_ += grid->size();
    grid_list_.push_back(std::move(grid));
  }

  // The calls to OverviewGrid::PrepareForOverview() requires some
  // LayoutManagers to perform layouts so that windows are correctly visible and
  // properly animated in overview mode. Otherwise these layouts should be
  // suppressed during overview mode so they don't conflict with overview mode
  // animations.

  // Do not call PrepareForOverview until all items are added to window_list_
  // as we don't want to cause any window updates until all windows in
  // overview are observed. See http://crbug.com/384495.
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    overview_grid->PrepareForOverview();

    // Do not animate if there is any window that is being dragged in the
    // grid.
    if (enter_exit_overview_type_ == EnterExitOverviewType::kImmediateEnter) {
      overview_grid->PositionWindows(/*animate=*/false);
    } else if (enter_exit_overview_type_ ==
               EnterExitOverviewType::kSlideInEnter) {
      overview_grid->PositionWindows(/*animate=*/false);
      overview_grid->SlideWindowsIn();
    } else {
      // Exit only types should not appear here:
      DCHECK_NE(enter_exit_overview_type_,
                EnterExitOverviewType::kSwipeFromShelf);
      DCHECK_NE(enter_exit_overview_type_,
                EnterExitOverviewType::kSlideOutExit);
      DCHECK_NE(enter_exit_overview_type_, EnterExitOverviewType::kFadeOutExit);

      overview_grid->PositionWindows(/*animate=*/true, /*ignored_items=*/{},
                                     OverviewTransition::kEnter);
    }
  }

  UpdateNoWindowsWidget();

  // Create the widget that will receive focus while in overview mode for
  // accessibility purposes.
  overview_focus_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.bounds = gfx::Rect(0, 0, 2, 2);
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewModeFocusedWidget";
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_StatusContainer);
  overview_focus_widget_->Init(std::move(params));

  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", num_items_);

  SplitViewController::Get(Shell::GetPrimaryRootWindow())->AddObserver(this);

  display::Screen::GetScreen()->AddObserver(this);
  base::RecordAction(base::UserMetricsAction("WindowSelector_Overview"));
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED);

  ignore_activations_ = false;
}

// NOTE: The work done in Shutdown() is not done in the destructor because it
// may cause other, unrelated classes, to make indirect calls to
// restoring_minimized_windows() on a partially destructed object.
void OverviewSession::Shutdown() {
  // This should have been set already when the process of ending overview mode
  // began. See OverviewController::OnSelectionEnded().
  DCHECK(is_shutting_down_);

  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->RemoveShellObserver(this);

  // Stop observing screen metrics changes first to avoid auto-positioning
  // windows in response to work area changes from window activation.
  display::Screen::GetScreen()->RemoveObserver(this);

  // Stop observing split view state changes before restoring window focus.
  // Otherwise the activation of the window triggers OnSplitViewStateChanged()
  // that will call into this function again.
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->RemoveObserver(this);

  size_t remaining_items = 0;
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    // During shutdown, do not animate all windows in overview if we need to
    // animate the snapped window.
    if (overview_grid->should_animate_when_exiting() &&
        enter_exit_overview_type_ != EnterExitOverviewType::kImmediateExit) {
      overview_grid->CalculateWindowListAnimationStates(
          selected_item_ &&
                  selected_item_->overview_grid() == overview_grid.get()
              ? selected_item_
              : nullptr,
          OverviewTransition::kExit, /*target_bounds=*/{});
    }
    for (const auto& overview_item : overview_grid->window_list())
      overview_item->RestoreWindow(/*reset_transform=*/true);
    remaining_items += overview_grid->size();
  }

  // Setting focus after restoring windows' state avoids unnecessary animations.
  // No need to restore if we are sliding to the home launcher screen, as all
  // windows will be minimized.
  const bool should_focus =
      enter_exit_overview_type_ == EnterExitOverviewType::kNormal ||
      enter_exit_overview_type_ == EnterExitOverviewType::kImmediateExit;
  ResetFocusRestoreWindow(should_focus);
  RemoveAllObservers();

  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_)
    overview_grid->Shutdown();

  DCHECK(num_items_ >= remaining_items);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.OverviewClosedItems",
                           num_items_ - remaining_items);
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowSelector.TimeInOverview",
                             base::Time::Now() - overview_start_time_);

  grid_list_.clear();

  if (no_windows_widget_) {
    if (enter_exit_overview_type_ == EnterExitOverviewType::kImmediateExit) {
      ImmediatelyCloseWidgetOnExit(std::move(no_windows_widget_));
      return;
    }

    // Fade out the no windows widget. This animation continues past the
    // lifetime of |this|.
    FadeOutWidgetAndMaybeSlideOnExit(std::move(no_windows_widget_),
                                     OVERVIEW_ANIMATION_RESTORE_WINDOW,
                                     /*slide=*/false);
  }
}

void OverviewSession::OnGridEmpty() {
  if (!IsEmpty())
    return;

  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InTabletSplitViewMode()) {
    UpdateNoWindowsWidget();
  } else {
    EndOverview();
  }
}

void OverviewSession::IncrementSelection(bool forward) {
  Move(/*reverse=*/!forward);
}

bool OverviewSession::AcceptSelection() {
  if (!highlight_controller_->GetHighlightedItem())
    return false;
  SelectWindow(highlight_controller_->GetHighlightedItem());
  return true;
}

void OverviewSession::SelectWindow(OverviewItem* item) {
  aura::Window* window = item->GetWindow();
  aura::Window::Windows window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (!window_list.empty()) {
    // Record WindowSelector_ActiveWindowChanged if the user is selecting a
    // window other than the window that was active prior to entering overview
    // mode (i.e., the window at the front of the MRU list).
    if (window_list[0] != window) {
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::OVERVIEW_MODE);
    }
    const auto it = std::find(window_list.begin(), window_list.end(), window);
    if (it != window_list.end()) {
      // Record 1-based index so that selecting a top MRU window will record 1.
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.SelectionDepth",
                               1 + it - window_list.begin());
    }
  }
  item->EnsureVisible();

  // If the selected window is a minimized window, un-minimize it first before
  // activating it so that the window can use the scale-up animation instead of
  // un-minimizing animation.
  if (WindowState::Get(window)->IsMinimized()) {
    ScopedAnimationDisabler disabler(window);
    WindowState::Get(window)->Unminimize();
    item->SetOpacity(1.f);
  }
  wm::ActivateWindow(window);
}

void OverviewSession::SetSplitViewDragIndicatorsDraggedWindow(
    aura::Window* dragged_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->SetSplitViewDragIndicatorsDraggedWindow(dragged_window);
}

void OverviewSession::UpdateSplitViewDragIndicatorsWindowDraggingStates(
    const aura::Window* root_window_being_dragged_in,
    bool is_dragging,
    SplitViewDragIndicators::WindowDraggingState non_snap_state,
    SplitViewController::SnapPosition snap_position) {
  using State = SplitViewDragIndicators::WindowDraggingState;
  const State window_dragging_state_on_root_window_being_dragged_in =
      SplitViewDragIndicators::ComputeWindowDraggingState(
          is_dragging, non_snap_state, snap_position);
  const State window_dragging_state_on_root_windows_not_being_dragged_in =
      SplitViewDragIndicators::ComputeWindowDraggingState(
          is_dragging, non_snap_state, SplitViewController::NONE);
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetSplitViewDragIndicatorsWindowDraggingState(
        grid->root_window() == root_window_being_dragged_in
            ? window_dragging_state_on_root_window_being_dragged_in
            : window_dragging_state_on_root_windows_not_being_dragged_in);
  }
}

void OverviewSession::RearrangeDuringDrag(aura::Window* dragged_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    DCHECK(grid->split_view_drag_indicators());
    grid->RearrangeDuringDrag(
        dragged_window,
        grid->split_view_drag_indicators()->current_window_dragging_state());
  }
}

OverviewGrid* OverviewSession::GetGridWithRootWindow(
    aura::Window* root_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->root_window() == root_window)
      return grid.get();
  }

  return nullptr;
}

void OverviewSession::AddItem(
    aura::Window* window,
    bool reposition,
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items,
    size_t index) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  grid->AddItem(window, reposition, animate, ignored_items, index);
  ++num_items_;

  UpdateNoWindowsWidget();

  // Transfer focus from |window| to |overview_focus_widget_| to match the
  // behavior of entering overview mode in the beginning.
  DCHECK(overview_focus_widget_);
  wm::ActivateWindow(GetOverviewFocusWindow());
}

void OverviewSession::AppendItem(aura::Window* window,
                                 bool reposition,
                                 bool animate) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  grid->AppendItem(window, reposition, animate, /*use_spawn_animation=*/true);
  ++num_items_;

  UpdateNoWindowsWidget();

  // Transfer focus from |window| to |overview_focus_widget_| to match the
  // behavior of entering overview mode in the beginning.
  DCHECK(overview_focus_widget_);
  wm::ActivateWindow(GetOverviewFocusWindow());
}

void OverviewSession::RemoveItem(OverviewItem* overview_item) {
  if (overview_item->GetWindow()->HasObserver(this)) {
    overview_item->GetWindow()->RemoveObserver(this);
    observed_windows_.erase(overview_item->GetWindow());
    if (overview_item->GetWindow() == restore_focus_window_)
      restore_focus_window_ = nullptr;
  }

  overview_item->overview_grid()->RemoveItem(overview_item);
  --num_items_;

  UpdateNoWindowsWidget();
}

void OverviewSession::InitiateDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen,
                                   bool is_touch_dragging) {
  if (Shell::Get()->overview_controller()->IsInStartAnimation() ||
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->IsDividerAnimating()) {
    return;
  }
  highlight_controller_->SetFocusHighlightVisibility(false);
  window_drag_controller_ = std::make_unique<OverviewWindowDragController>(
      this, item, is_touch_dragging);
  window_drag_controller_->InitiateDrag(location_in_screen);

  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragStarted(item);
}

void OverviewSession::Drag(OverviewItem* item,
                           const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->Drag(location_in_screen);
}

void OverviewSession::CompleteDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  const bool snap =
      window_drag_controller_->CompleteDrag(location_in_screen) ==
      OverviewWindowDragController::DragResult::kSuccessfulDragToSnap;
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded(snap);

  highlight_controller_->SetFocusHighlightVisibility(true);
}

void OverviewSession::StartNormalDragMode(
    const gfx::PointF& location_in_screen) {
  window_drag_controller_->StartNormalDragMode(location_in_screen);
}

void OverviewSession::Fling(OverviewItem* item,
                            const gfx::PointF& location_in_screen,
                            float velocity_x,
                            float velocity_y) {
  // Its possible a fling event is not paired with a tap down event. Ignore
  // these flings.
  if (!window_drag_controller_ || item != window_drag_controller_->item())
    return;

  const bool snap =
      window_drag_controller_->Fling(location_in_screen, velocity_x,
                                     velocity_y) ==
      OverviewWindowDragController::DragResult::kSuccessfulDragToSnap;
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded(snap);
}

void OverviewSession::ActivateDraggedWindow() {
  window_drag_controller_->ActivateDraggedWindow();
}

void OverviewSession::ResetDraggedWindowGesture() {
  window_drag_controller_->ResetGesture();
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded(/*snap=*/false);
}

void OverviewSession::OnWindowDragStarted(aura::Window* dragged_window,
                                          bool animate) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragStarted(dragged_window, animate);
}

void OverviewSession::OnWindowDragContinued(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen,
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragContinued(dragged_window, location_in_screen,
                                     window_dragging_state);
}

void OverviewSession::OnWindowDragEnded(aura::Window* dragged_window,
                                        const gfx::PointF& location_in_screen,
                                        bool should_drop_window_into_overview,
                                        bool snap) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragEnded(dragged_window, location_in_screen,
                                 should_drop_window_into_overview, snap);
}

void OverviewSession::SetVisibleDuringWindowDragging(bool visible,
                                                     bool animate) {
  for (auto& grid : grid_list_)
    grid->SetVisibleDuringWindowDragging(visible, animate);
}

void OverviewSession::PositionWindows(
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->PositionWindows(animate, ignored_items);

  RefreshNoWindowsWidgetBounds(animate);
}

bool OverviewSession::IsWindowInOverview(const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetOverviewItemContaining(window))
      return true;
  }
  return false;
}

OverviewItem* OverviewSession::GetOverviewItemForWindow(
    const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    OverviewItem* item = grid->GetOverviewItemContaining(window);
    if (item)
      return item;
  }

  return nullptr;
}

void OverviewSession::SetWindowListNotAnimatedWhenExiting(
    aura::Window* root_window) {
  // Find the grid accociated with |root_window|.
  OverviewGrid* grid = GetGridWithRootWindow(root_window);
  if (grid)
    grid->SetWindowListNotAnimatedWhenExiting();
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
OverviewSession::UpdateGridAtLocationYPositionAndOpacity(
    int64_t display_id,
    int new_y,
    float opacity,
    UpdateAnimationSettingsCallback callback) {
  OverviewGrid* grid = GetGridWithRootWindow(
      Shell::Get()->GetRootWindowForDisplayId(display_id));
  if (!grid)
    return nullptr;

  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings =
      grid->UpdateYPositionAndOpacity(new_y, opacity, callback);
  if (no_windows_widget_) {
    DCHECK(grid->empty());

    // Translate and fade |no_windows_widget_| if it is visible. Overwrite
    // |settings| to observe |no_windows_widget| if necessary.
    aura::Window* window = no_windows_widget_->GetNativeWindow();
    ui::Layer* layer = window->layer();
    if (!callback.is_null()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer->GetAnimator());
      callback.Run(settings.get());
    }
    window->SetTransform(gfx::Transform(1.f, 0.f, 0.f, 1.f, 0.f, -new_y));
    layer->SetOpacity(opacity);
  }

  return settings;
}

void OverviewSession::UpdateRoundedCornersAndShadow() {
  for (auto& grid : grid_list_)
    for (auto& window : grid->window_list())
      window->UpdateRoundedCornersAndShadow();
}

void OverviewSession::OnStartingAnimationComplete(bool canceled,
                                                  bool should_focus_overview) {
  for (auto& grid : grid_list_)
    grid->OnStartingAnimationComplete(canceled);

  if (canceled)
    return;
  if (overview_focus_widget_) {
    if (should_focus_overview) {
      overview_focus_widget_->Show();
    } else {
      overview_focus_widget_->ShowInactive();

      // Check if the active window is in overview. There is at least one
      // workflow where it will be: the active window is being dragged, and the
      // previous window carries over from clamshell mode to tablet split view.
      if (IsWindowInOverview(window_util::GetActiveWindow())) {
        // We do not want an active window in overview. It will cause blatantly
        // broken behavior as in the video linked in crbug.com/992223.
        wm::ActivateWindow(
            SplitViewController::Get(Shell::GetPrimaryRootWindow())
                ->GetDefaultSnappedWindow());
      }
    }
  }
  Shell::Get()->overview_controller()->DelayedUpdateRoundedCornersAndShadow();
}

void OverviewSession::OnWindowActivating(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (ignore_activations_ || gained_active == GetOverviewFocusWindow())
    return;

  if (features::IsVirtualDesksEnabled() &&
      DesksController::Get()->AreDesksBeingModified()) {
    // Activating a desk from its mini view will activate its most-recently used
    // window, but this should not result in ending overview mode now.
    // Overview will be ended explicitly as part of the desk activation
    // animation.
    return;
  }

  if (!gained_active) {
    // Cancel overview session and do not restore focus when active window is
    // set to nullptr. This happens when removing a display.
    ResetFocusRestoreWindow(false);
    EndOverview();
    return;
  }

  // If app list is open in clamshell mode, end overview. Note: we have special
  // logic to end overview when app list (i.e., home launcher) is open in tablet
  // mode, so do not handle it here.
  if (gained_active == Shell::Get()->app_list_controller()->GetWindow() &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    ResetFocusRestoreWindow(false);
    EndOverview();
    return;
  }

  // Do not cancel overview mode if the window activation happens when split
  // view mode is also active. SplitViewController will do the right thing to
  // handle the window activation change. Check for split view mode without
  // using |SplitViewController::state_| which is updated asynchronously when
  // snapping an ARC window.
  SplitViewController* split_view_controller =
      SplitViewController::Get(gained_active);
  if (split_view_controller->left_window() ||
      split_view_controller->right_window()) {
    return;
  }

  // Do not cancel overview mode if the window activation was caused while
  // dragging overview mode offscreen.
  if (IsSlidingOutOverviewFromShelf())
    return;

  // Do not cancel overview mode while a window or overview item is being
  // dragged as evidenced by the presence of a drop target. (Dragging to close
  // does not count; canceling overview mode is okay then.)
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    if (overview_grid->GetDropTarget())
      return;
  }

  auto* grid = GetGridWithRootWindow(gained_active->GetRootWindow());
  DCHECK(grid);
  const auto& windows = grid->window_list();
  auto iter = std::find_if(
      windows.begin(), windows.end(),
      [gained_active](const std::unique_ptr<OverviewItem>& window) {
        return window->Contains(gained_active);
      });

  if (iter != windows.end())
    selected_item_ = iter->get();

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  EndOverview();
}

aura::Window* OverviewSession::GetOverviewFocusWindow() {
  if (overview_focus_widget_)
    return overview_focus_widget_->GetNativeWindow();

  return nullptr;
}

aura::Window* OverviewSession::GetHighlightedWindow() {
  OverviewItem* item = highlight_controller_->GetHighlightedItem();
  if (!item)
    return nullptr;
  return item->GetWindow();
}

void OverviewSession::SuspendReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(true);
}

void OverviewSession::ResumeReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(false);
}

bool OverviewSession::IsEmpty() const {
  for (const auto& grid : grid_list_) {
    if (!grid->empty())
      return false;
  }
  return true;
}

void OverviewSession::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;

  if (features::IsVirtualDesksEnabled()) {
    // Do not restore focus to a window that exists on an inactive desk.
    focus &= base::Contains(DesksController::Get()->active_desk()->windows(),
                            restore_focus_window_);
  }

  // Ensure the window is still in the window hierarchy and not in the middle
  // of teardown.
  if (focus && restore_focus_window_->GetRootWindow()) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    wm::ActivateWindow(restore_focus_window_);
  }
  // If the window is in the observed_windows_ list it needs to continue to be
  // observed.
  if (observed_windows_.find(restore_focus_window_) ==
      observed_windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = nullptr;
}

void OverviewSession::OnHighlightedItemActivated(OverviewItem* item) {
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ArrowKeyPresses",
                           num_key_presses_);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.WindowSelector.KeyPressesOverItemsRatio",
                              (num_key_presses_ * 100) / num_items_, 1, 300,
                              30);
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
  SelectWindow(item);
}

void OverviewSession::OnHighlightedItemClosed(OverviewItem* item) {
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseKey"));
  item->CloseWindow();
}

void OverviewSession::OnDisplayAdded(const display::Display& display) {
  EndOverview();
}

void OverviewSession::OnDisplayRemoved(const display::Display& display) {
  // Removing a display causes a window activation which will end overview mode
  // so that |OnDisplayRemoved| is never called.
  NOTREACHED();
}

void OverviewSession::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  if (window_drag_controller_)
    ResetDraggedWindowGesture();
  GetGridWithRootWindow(Shell::GetRootWindowForDisplayId(display.id()))
      ->OnDisplayMetricsChanged();

  // The no windows widget is on the primary root window. If |display|
  // corresponds to another root window, then we are done.
  if (display.id() !=
      GetRootWindowSettings(Shell::GetPrimaryRootWindow())->display_id) {
    return;
  }
  // In case of split view mode, the no windows widget bounds will be updated in
  // |OnSplitViewDividerPositionChanged|.
  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InSplitViewMode()) {
    return;
  }
  RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewSession::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Only care about newly added children of |observed_windows_|.
  if (!observed_windows_.count(params.receiver) ||
      !observed_windows_.count(params.new_parent)) {
    return;
  }

  // Removing a desk while in overview mode results in reparenting the windows
  // of that desk to the associated container of another desk. This is a window
  // hierarchy change that shouldn't result in exiting overview mode.
  if (features::IsVirtualDesksEnabled() &&
      DesksController::Get()->AreDesksBeingModified()) {
    return;
  }

  aura::Window* new_window = params.target;
  WindowState* state = WindowState::Get(new_window);
  if (!state->IsUserPositionable() || state->IsPip())
    return;

  // If the new window is added when splitscreen is active, do nothing.
  // SplitViewController will do the right thing to snap the window or end
  // overview mode.
  if (SplitViewController::Get(new_window)->InSplitViewMode())
    return;

  if (IsSwitchableContainer(new_window->parent()) &&
      !::wm::GetTransientParent(new_window)) {
    // The new window is in one of the switchable containers, abort overview.
    EndOverview();
    return;
  }
}

void OverviewSession::OnWindowDestroying(aura::Window* window) {
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_OVERVIEW_WINDOW_CLOSING_A11Y_ALERT, window->GetTitle()));

  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = nullptr;
}

void OverviewSession::OnKeyEvent(ui::KeyEvent* event) {
  // If app list is open when overview is active (it can happen in clamshell
  // mode, when we snap an overview window to one side of the screen and then
  // open the app list to select an app to snap to the other side), in this case
  // we let the app list to handle the key event.
  // TODO(crbug.com/952315): Explore better ways to handle this splitview +
  // overview + applist case.
  Shell* shell = Shell::Get();
  if (!shell->tablet_mode_controller()->InTabletMode() &&
      shell->app_list_controller()->IsVisible()) {
    return;
  }

  const bool process_released_key_event =
      (event->key_code() == ui::VKEY_LEFT ||
       event->key_code() == ui::VKEY_RIGHT) &&
      ShouldUseTabletModeGridLayout();
  if (event->type() != ui::ET_KEY_PRESSED && !process_released_key_event)
    return;

  switch (event->key_code()) {
    case ui::VKEY_BROWSER_BACK:
    case ui::VKEY_ESCAPE:
      EndOverview();
      break;
    case ui::VKEY_UP:
      ++num_key_presses_;
      Move(/*reverse=*/true);
      break;
    case ui::VKEY_DOWN:
      ++num_key_presses_;
      Move(/*reverse=*/false);
      break;
    case ui::VKEY_RIGHT:
      if (!ProcessForScrolling(*event)) {
        ++num_key_presses_;
        Move(/*reverse=*/false);
      }
      break;
    case ui::VKEY_TAB: {
      const bool reverse = event->flags() & ui::EF_SHIFT_DOWN;
      ++num_key_presses_;
      Move(reverse);
      break;
    }
    case ui::VKEY_LEFT:
      if (!ProcessForScrolling(*event)) {
        ++num_key_presses_;
        Move(/*reverse=*/true);
      }
      break;
    case ui::VKEY_W: {
      if (!(event->flags() & ui::EF_CONTROL_DOWN))
        return;

      if (!highlight_controller_->MaybeCloseHighlightedView())
        return;
      break;
    }
    case ui::VKEY_RETURN: {
      if (!highlight_controller_->MaybeActivateHighlightedView())
        return;
      break;
    }
    default:
      return;
  }

  event->SetHandled();
  event->StopPropagation();
}

void OverviewSession::OnShellDestroying() {
  // Cancel selection will call |Shutdown()|, which will remove observer.
  EndOverview();
}

void OverviewSession::OnShelfAlignmentChanged(aura::Window* root_window) {
  // When the shelf alignment changes while in overview, the display work area
  // doesn't get updated anyways (see https://crbug.com/834400). In this case,
  // even updating the grid bounds won't make any difference, so we simply exit
  // overview.
  EndOverview();
}

void OverviewSession::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // Do nothing if overview is being shutdown.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewSession::OnSplitViewDividerPositionChanged() {
  RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewSession::Move(bool reverse) {
  // Do not allow moving the highlight while in the middle of a drag.
  if (window_drag_controller_ && window_drag_controller_->item())
    return;

  highlight_controller_->MoveHighlight(reverse);
}

bool OverviewSession::ProcessForScrolling(const ui::KeyEvent& event) {
  if (!ShouldUseTabletModeGridLayout() ||
      !(event.flags() & ui::EF_CONTROL_DOWN)) {
    return false;
  }

  const bool press = (event.type() == ui::ET_KEY_PRESSED);
  const bool repeat = event.is_repeat();
  DCHECK(event.key_code() == ui::VKEY_LEFT ||
         event.key_code() == ui::VKEY_RIGHT);
  const bool reverse = event.key_code() == ui::VKEY_LEFT;

  // TODO(sammiequon): This only works for tablet mode at the moment, so using
  // the primary display works. If this feature is adapted for multi display
  // then this needs to be revisited.
  auto* grid = GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  if (press && !repeat) {
    grid->StartScroll();
    grid->UpdateScrollOffset(kKeyboardPressScrollingDp * (reverse ? 1 : -1));
    return true;
  }

  if (press && repeat) {
    grid->UpdateScrollOffset(kKeyboardHoldScrollingDp * (reverse ? 1 : -1));
    return true;
  }

  grid->EndScroll();
  return true;
}

void OverviewSession::RemoveAllObservers() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  observed_windows_.clear();

  display::Screen::GetScreen()->RemoveObserver(this);
  if (restore_focus_window_)
    restore_focus_window_->RemoveObserver(this);
}

void OverviewSession::UpdateNoWindowsWidget() {
  if (is_shutting_down_)
    return;

  // Hide the widget if there is an item in overview.
  if (!IsEmpty()) {
    no_windows_widget_.reset();
    return;
  }

  if (!no_windows_widget_) {
    // Create and fade in the widget.
    RoundedLabelWidget::InitParams params;
    params.name = "OverviewNoWindowsLabel";
    params.horizontal_padding = kNoItemsIndicatorHorizontalPaddingDp;
    params.vertical_padding = kNoItemsIndicatorVerticalPaddingDp;
    params.background_color = kNoItemsIndicatorBackgroundColor;
    params.foreground_color = kNoItemsIndicatorTextColor;
    params.rounding_dp = kNoItemsIndicatorRoundingDp;
    params.preferred_height = kNoItemsIndicatorHeightDp;
    params.message_id = IDS_ASH_OVERVIEW_NO_RECENT_ITEMS;
    params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        desks_util::GetActiveDeskContainerId());
    params.hide_in_mini_view = true;
    no_windows_widget_ = std::make_unique<RoundedLabelWidget>();
    no_windows_widget_->Init(std::move(params));

    aura::Window* widget_window = no_windows_widget_->GetNativeWindow();
    widget_window->parent()->StackChildAtBottom(widget_window);
    ScopedOverviewAnimationSettings settings(OVERVIEW_ANIMATION_NO_RECENTS_FADE,
                                             widget_window);
    no_windows_widget_->SetOpacity(1.f);
  }

  RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewSession::RefreshNoWindowsWidgetBounds(bool animate) {
  if (!no_windows_widget_)
    return;

  auto* grid = GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  DCHECK(grid);
  no_windows_widget_->SetBoundsCenteredIn(grid->GetGridEffectiveBounds(),
                                          animate);
}

}  // namespace ash
