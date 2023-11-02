// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_metrics.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The threshold to compute the minimum vertical distance to start showing the
// drag indicators and preview window when dragging a window into splitscreen in
// tablet mode.
constexpr float kIndicatorsThresholdRatio = 0.1;

// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragToSnapDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;
// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;

// Duration of a drag that it will be considered as an intended drag. Must be at
// least the duration of the split view divider snap animation, or else issues
// like crbug.com/946601, crbug.com/997764, and https://crbug.com/997765, which
// all refer to dragging from overview, will apply to dragging from the top.
constexpr base::TimeDelta kIsWindowMovedTimeoutMs = base::Milliseconds(300);

constexpr char kSwipeDownDragWindowHistogram[] =
    "Ash.SwipeDownDrag.Window.PresentationTime.TabletMode";
constexpr char kSwipeDownDragWindowMaxLatencyHistogram[] =
    "Ash.SwipeDownDrag.Window.PresentationTime.MaxLatency.TabletMode";
constexpr char kSwipeDownDragTabHistogram[] =
    "Ash.SwipeDownDrag.Tab.PresentationTime.TabletMode";
constexpr char kSwipeDownDragTabMaxLatencyHistogram[] =
    "Ash.SwipeDownDrag.Tab.PresentationTime.MaxLatency.TabletMode";

// Returns the overview session if overview mode is active, otherwise returns
// nullptr.
OverviewSession* GetOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession()
             ? Shell::Get()->overview_controller()->overview_session()
             : nullptr;
}

OverviewGrid* GetOverviewGrid(aura::Window* dragged_window) {
  if (!GetOverviewSession())
    return nullptr;

  return GetOverviewSession()->GetGridWithRootWindow(
      dragged_window->GetRootWindow());
}

// Returns the drop target in overview during drag.
OverviewItem* GetDropTarget(aura::Window* dragged_window) {
  OverviewGrid* overview_grid = GetOverviewGrid(dragged_window);
  if (!overview_grid || overview_grid->empty())
    return nullptr;

  return overview_grid->GetDropTarget();
}

// Gets the bounds of selected drop target in overview grid that is displaying
// in the same root window as |dragged_window|. Note that the returned bounds is
// scaled-up.
gfx::Rect GetBoundsOfSelectedDropTarget(aura::Window* dragged_window) {
  OverviewItem* drop_target = GetDropTarget(dragged_window);
  if (!drop_target)
    return gfx::Rect();

  return drop_target->GetBoundsOfSelectedItem();
}

}  // namespace

TabletModeWindowDragDelegate::TabletModeWindowDragDelegate()
    : split_view_controller_(
          SplitViewController::Get(Shell::GetPrimaryRootWindow())),
      split_view_drag_indicators_(std::make_unique<SplitViewDragIndicators>(
          Shell::GetPrimaryRootWindow())) {}

TabletModeWindowDragDelegate::~TabletModeWindowDragDelegate() {
  if (dragged_window_) {
    OverviewGrid* overview_grid = GetOverviewGrid(dragged_window_);
    if (overview_grid) {
      overview_grid->RemoveDropTarget();
      const gfx::Rect grid_bounds = GetGridBoundsInScreen(dragged_window_);
      overview_grid->SetBoundsAndUpdatePositions(
          grid_bounds, /*ignored_items=*/{}, /*animate=*/true);
    }
  }

  split_view_controller_->OnWindowDragCanceled();
  Shelf::UpdateShelfVisibility();
}

void TabletModeWindowDragDelegate::StartWindowDrag(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen) {
  // TODO(oshima): Consider doing the same for normal window dragging as well
  // crbug.com/904631.
  DCHECK(!occlusion_excluder_);
  occlusion_excluder_.emplace(dragged_window);

  DCHECK(!presentation_time_recorder_);
  presentation_time_recorder_.reset();
  if (window_util::IsDraggingTabs(dragged_window)) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        dragged_window->layer()->GetCompositor(), kSwipeDownDragTabHistogram,
        kSwipeDownDragTabMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        dragged_window->layer()->GetCompositor(), kSwipeDownDragWindowHistogram,
        kSwipeDownDragWindowMaxLatencyHistogram);
  }

  dragged_window_ = dragged_window;
  initial_location_in_screen_ = location_in_screen;
  drag_start_deadline_ = base::Time::Now() + kIsWindowMovedTimeoutMs;

  PrepareWindowDrag(location_in_screen);

  // Update the shelf's visibility to keep shelf visible during drag.
  RootWindowController::ForWindow(dragged_window_)
      ->GetShelfLayoutManager()
      ->UpdateVisibilityState();

  // Disable the backdrop on the dragged window.
  WindowBackdrop::Get(dragged_window_)->DisableBackdrop();

  OverviewController* controller = Shell::Get()->overview_controller();
  bool was_overview_open = controller->InOverviewSession();

  const bool was_splitview_active = split_view_controller_->InSplitViewMode();
  // If the dragged window is one of the snapped windows, SplitViewController
  // might open overview in the dragged window side of the screen.
  split_view_controller_->OnWindowDragStarted(dragged_window_);
  if (ShouldOpenOverviewWhenDragStarts() && !controller->InOverviewSession()) {
    OverviewButtonTray* overview_button_tray =
        RootWindowController::ForWindow(dragged_window_)
            ->GetStatusAreaWidget()
            ->overview_button_tray();
    DCHECK(overview_button_tray);
    overview_button_tray->SnapRippleToActivated();
    controller->StartOverview(OverviewStartAction::kSplitView,
                              OverviewEnterExitType::kImmediateEnter);
  }

  if (controller->InOverviewSession()) {
    // Only do animation if overview was open before the drag started. If the
    // overview is opened because of the window drag, do not do animation.
    GetOverviewSession()->OnWindowDragStarted(dragged_window_,
                                              /*animate=*/was_overview_open);
  }

  split_view_drag_indicators_->SetDraggedWindow(dragged_window_);
  bounds_of_selected_drop_target_ =
      GetBoundsOfSelectedDropTarget(dragged_window_);

  // Update the dragged window's shadow. It should have the active window
  // shadow during dragging.
  original_shadow_elevation_ =
      ::wm::GetShadowElevationConvertDefault(dragged_window_);
  ::wm::SetShadowElevation(dragged_window_, ::wm::kShadowElevationActiveWindow);

  Shell* shell = Shell::Get();
  TabletModeController* tablet_mode_controller =
      shell->tablet_mode_controller();
  if (window_util::IsDraggingTabs(dragged_window_)) {
    tablet_mode_controller->increment_tab_drag_count();
    if (was_splitview_active)
      tablet_mode_controller->increment_tab_drag_in_splitview_count();

    // For tab drag, we only open the overview behind if the dragged window is
    // the source window.
    if (ShouldOpenOverviewWhenDragStarts())
      RecordTabDragTypeHistogram(TabDragType::kDragSourceWindow);
    else
      RecordTabDragTypeHistogram(TabDragType::kDragTabOutOfWindow);
  } else {
    tablet_mode_controller->increment_app_window_drag_count();
    if (was_splitview_active)
      tablet_mode_controller->increment_app_window_drag_in_splitview_count();
  }
  if (controller->InOverviewSession()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Tablet.WindowDrag.OpenedWindowsNumber",
        shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk).size());
    base::RecordAction(
        base::UserMetricsAction("Tablet.WindowDrag.OpenedOverview"));
  }
}

void TabletModeWindowDragDelegate::ContinueWindowDrag(
    const gfx::PointF& location_in_screen,
    UpdateDraggedWindowType type,
    const gfx::Rect& target_bounds) {
  UpdateIsWindowConsideredMoved(location_in_screen.y());

  if (presentation_time_recorder_)
    presentation_time_recorder_->RequestNext();

  if (type == UpdateDraggedWindowType::UPDATE_BOUNDS) {
    // UPDATE_BOUNDS is used when dragging tab(s) out of a browser window.
    // Changing bounds might delete ourselves as the dragged (browser) window
    // tab(s) might merge into another browser window.
    base::WeakPtr<TabletModeWindowDragDelegate> delegate(
        weak_ptr_factory_.GetWeakPtr());
    if (target_bounds != dragged_window_->bounds()) {
      dragged_window_->SetBounds(target_bounds);
      if (!delegate)
        return;
    }
  } else {  // type == UpdateDraggedWindowType::UPDATE_TRANSFORM
    // UPDATE_TRANSFORM is used when dragging an entire window around, either
    // it's a browser window or an app window.
    UpdateDraggedWindowTransform(location_in_screen);
  }

  // For child classes to do their special handling if any.
  UpdateWindowDrag(location_in_screen);

  // Update drag indicators and preview window if necessary.
  SplitViewDragIndicators::WindowDraggingState window_dragging_state =
      SplitViewDragIndicators::ComputeWindowDraggingState(
          is_window_considered_moved_,
          SplitViewDragIndicators::WindowDraggingState::kFromTop,
          GetSnapPosition(location_in_screen));
  split_view_drag_indicators_->SetWindowDraggingState(window_dragging_state);

  if (GetOverviewSession()) {
    GetOverviewSession()->OnWindowDragContinued(
        dragged_window_, location_in_screen, window_dragging_state);
  }
}

void TabletModeWindowDragDelegate::EndWindowDrag(
    ToplevelWindowEventHandler::DragResult result,
    const gfx::PointF& location_in_screen) {
  EndingWindowDrag(result, location_in_screen);

  WindowBackdrop::Get(dragged_window_)->RestoreBackdrop();
  SplitViewController::SnapPosition snap_position =
      SplitViewController::SnapPosition::kNone;
  if (result == ToplevelWindowEventHandler::DragResult::SUCCESS &&
      split_view_controller_->CanSnapWindow(dragged_window_)) {
    snap_position = GetSnapPosition(location_in_screen);
  }

  // The window might merge into an overview window or become a new window item
  // in overview mode.
  OverviewSession* overview_session = GetOverviewSession();
  if (overview_session) {
    GetOverviewSession()->OnWindowDragEnded(
        dragged_window_, location_in_screen,
        ShouldDropWindowIntoOverview(snap_position, location_in_screen),
        snap_position != SplitViewController::SnapPosition::kNone);
  }

  WindowState::Get(dragged_window_)
      ->set_snap_action_source(WindowSnapActionSource::kDragDownFromTopToSnap);
  split_view_controller_->OnWindowDragEnded(
      dragged_window_, snap_position, gfx::ToRoundedPoint(location_in_screen));
  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);

  // Reset the dragged window's window shadow elevation.
  ::wm::SetShadowElevation(dragged_window_, original_shadow_elevation_);

  // For child class to do its special handling if any.
  EndedWindowDrag(location_in_screen);

  if (!window_util::IsDraggingTabs(dragged_window_)) {
    if (split_view_controller_->IsWindowInSplitView(dragged_window_)) {
      RecordAppDragEndWindowStateHistogram(
          AppWindowDragEndWindowState::kDraggedIntoSplitView);
    } else if (overview_session &&
               overview_session->IsWindowInOverview(dragged_window_)) {
      RecordAppDragEndWindowStateHistogram(
          AppWindowDragEndWindowState::kDraggedIntoOverview);
    } else {
      RecordAppDragEndWindowStateHistogram(
          AppWindowDragEndWindowState::kBackToMaximizedOrFullscreen);
    }
  }

  presentation_time_recorder_.reset();
  occlusion_excluder_.reset();
  dragged_window_ = nullptr;
  is_window_considered_moved_ = false;
}

void TabletModeWindowDragDelegate::FlingOrSwipe(ui::GestureEvent* event) {
  if (event->type() == ui::ET_SCROLL_FLING_START) {
    if (ShouldFlingIntoOverview(event)) {
      DCHECK(Shell::Get()->overview_controller()->InOverviewSession());
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->AddItemInMruOrder(dragged_window_, /*reposition=*/true,
                              /*animate=*/false, /*restack=*/true,
                              /*use_spawn_animation=*/false);
    }
    StartFling(event);
  }
  EndWindowDrag(ToplevelWindowEventHandler::DragResult::SUCCESS,
                GetEventLocationInScreen(event));
}

gfx::PointF TabletModeWindowDragDelegate::GetEventLocationInScreen(
    const ui::GestureEvent* event) const {
  gfx::PointF location_in_screen(event->location());
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &location_in_screen);
  return location_in_screen;
}

bool TabletModeWindowDragDelegate::ShouldOpenOverviewWhenDragStarts() {
  DCHECK(dragged_window_);
  return true;
}

int TabletModeWindowDragDelegate::GetIndicatorsVerticalThreshold(
    const gfx::Rect& work_area_bounds) const {
  return work_area_bounds.y() +
         work_area_bounds.height() * kIndicatorsThresholdRatio;
}

SplitViewController::SnapPosition TabletModeWindowDragDelegate::GetSnapPosition(
    const gfx::PointF& location_in_screen) const {
  // Check that the window has been considered as moved and is compatible with
  // split view. If the window has not been considered as moved, it shall not
  // become snapped, although if it already was snapped, it can stay snapped.
  if (!(is_window_considered_moved_ &&
        split_view_controller_->CanSnapWindow(dragged_window_))) {
    return SplitViewController::SnapPosition::kNone;
  }

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          dragged_window_);
  SplitViewController::SnapPosition snap_position = ::ash::GetSnapPosition(
      Shell::GetPrimaryRootWindow(), dragged_window_,
      gfx::ToRoundedPoint(location_in_screen),
      gfx::ToRoundedPoint(initial_location_in_screen_),
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);

  // For portrait mode, since the drag always starts from the top of the
  // screen, we only allow the window to be dragged to snap to the bottom of
  // the screen.
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const bool is_primary = IsCurrentScreenOrientationPrimary();
  if (!is_landscape &&
      ((is_primary &&
        snap_position == SplitViewController::SnapPosition::kPrimary) ||
       (!is_primary &&
        snap_position == SplitViewController::SnapPosition::kSecondary))) {
    snap_position = SplitViewController::SnapPosition::kNone;
  }

  return snap_position;
}

void TabletModeWindowDragDelegate::UpdateDraggedWindowTransform(
    const gfx::PointF& location_in_screen) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  // Calculate the desired scale along the y-axis. The scale of the window
  // during drag is based on the distance from |y_location_in_screen| to the y
  // position of |bounds_of_selected_drop_target_|. The dragged windowt will
  // become smaller when it becomes nearer to the drop target. And then keep the
  // minimum scale if it has been dragged further than the drop target.
  float scale = static_cast<float>(bounds_of_selected_drop_target_.height()) /
                static_cast<float>(dragged_window_->bounds().height());
  float y_full = bounds_of_selected_drop_target_.y();
  float y_diff = y_full - location_in_screen.y();
  if (y_diff >= 0)
    scale = (1.0f - scale) * y_diff / y_full + scale;

  gfx::Transform transform;
  gfx::Rect window_bounds = dragged_window_->bounds();
  ::wm::ConvertRectToScreen(dragged_window_->parent(), &window_bounds);
  transform.Translate(
      (location_in_screen.x() - window_bounds.x()) -
          (initial_location_in_screen_.x() - window_bounds.x()) * scale,
      (location_in_screen.y() - window_bounds.y()) -
          (initial_location_in_screen_.y() - window_bounds.y()) * scale);
  transform.Scale(scale, scale);
  SetTransform(dragged_window_, transform);
}

bool TabletModeWindowDragDelegate::ShouldDropWindowIntoOverview(
    SplitViewController::SnapPosition snap_position,
    const gfx::PointF& location_in_screen) {
  // Do not drop the dragged window into overview if preview area is shown.
  if (snap_position != SplitViewController::SnapPosition::kNone)
    return false;

  OverviewItem* drop_target = GetDropTarget(dragged_window_);
  if (!drop_target)
    return false;

  OverviewGrid* overview_grid = GetOverviewGrid(dragged_window_);
  aura::Window* target_window = overview_grid->GetTargetWindowOnLocation(
      location_in_screen, /*ignored_item=*/nullptr);
  const bool is_drop_target_selected =
      target_window && overview_grid->IsDropTargetWindow(target_window);

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  return is_drop_target_selected ||
         (location_in_screen.y() - work_area_bounds.y()) >=
             kDragPositionToOverviewRatio *
                 (drop_target->GetTransformedBounds().y() -
                  work_area_bounds.y());
}

bool TabletModeWindowDragDelegate::ShouldFlingIntoOverview(
    const ui::GestureEvent* event) const {
  // Only fling into overview if overview is currently open. In some case,
  // overview is not opened when drag starts (if it's tab-dragging and the
  // dragged window is not the same with the source window), we should not fling
  // the dragged window into overview in this case.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return false;

  const gfx::PointF location_in_screen = GetEventLocationInScreen(event);
  const SplitViewDragIndicators::WindowDraggingState window_dragging_state =
      SplitViewDragIndicators::ComputeWindowDraggingState(
          is_window_considered_moved_,
          SplitViewDragIndicators::WindowDraggingState::kFromTop,
          GetSnapPosition(location_in_screen));
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const float velocity = is_landscape ? event->details().velocity_x()
                                      : event->details().velocity_y();

  // Drop the window into overview if fling with large enough velocity to the
  // opposite snap position when preview area is shown.
  if (IsCurrentScreenOrientationPrimary()) {
    if (window_dragging_state ==
        SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary) {
      return velocity > kFlingToOverviewFromSnappingAreaThreshold;
    } else if (window_dragging_state ==
               SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary) {
      return -velocity > kFlingToOverviewFromSnappingAreaThreshold;
    }
  } else {
    if (window_dragging_state ==
        SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary) {
      return -velocity > kFlingToOverviewFromSnappingAreaThreshold;
    } else if (window_dragging_state ==
               SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary) {
      return velocity > kFlingToOverviewFromSnappingAreaThreshold;
    }
  }

  // Consider only the velocity_y if no preview area is shown.
  return event->details().velocity_y() > kFlingToOverviewThreshold;
}

void TabletModeWindowDragDelegate::UpdateIsWindowConsideredMoved(
    int y_location_in_screen) {
  if (is_window_considered_moved_)
    return;

  if (base::Time::Now() < drag_start_deadline_)
    return;

  DCHECK(dragged_window_);
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  is_window_considered_moved_ =
      y_location_in_screen >= GetIndicatorsVerticalThreshold(work_area_bounds);
}

}  // namespace ash
