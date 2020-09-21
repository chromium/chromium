// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/drag_window_from_shelf_controller.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/home_screen/window_scale_animation.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_property.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The minimum window scale factor when dragging a window from shelf.
constexpr float kMinimumWindowScaleDuringDragging = 0.3f;

// The ratio in display height at which point the dragged window shrinks to its
// minimum scale kMinimumWindowScaleDuringDragging.
constexpr float kMinYDisplayHeightRatio = 0.125f;

// Amount of time to wait to show overview after the user slows down or stops
// window dragging.
constexpr base::TimeDelta kShowOverviewTimeWhenDragSuspend =
    base::TimeDelta::FromMilliseconds(40);

// The scroll update threshold to restart the show overview timer.
constexpr float kScrollUpdateOverviewThreshold = 2.f;

// Presentation time histogram names.
constexpr char kDragWindowFromShelfHistogram[] =
    "Ash.DragWindowFromShelf.PresentationTime";
constexpr char kDragWindowFromShelfMaxLatencyHistogram[] =
    "Ash.DragWindowFromShelf.PresentationTime.MaxLatency";

}  // namespace

// Hide all visible windows expect the dragged windows or the window showing in
// splitview during dragging.
class DragWindowFromShelfController::WindowsHider
    : public aura::WindowObserver {
 public:
  explicit WindowsHider(aura::Window* dragged_window)
      : dragged_window_(dragged_window) {
    std::vector<aura::Window*> windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
    for (auto* window : windows) {
      if (window == dragged_window_)
        continue;
      if (::wm::HasTransientAncestor(window, dragged_window_))
        continue;
      if (!window->IsVisible())
        continue;
      if (SplitViewController::Get(window)->IsWindowInSplitView(window))
        continue;

      hidden_windows_.push_back(window);
      window->AddObserver(this);
      window->SetProperty(kHideDuringWindowDragging, true);
    }
    window_util::MinimizeAndHideWithoutAnimation(hidden_windows_);
  }

  ~WindowsHider() override {
    for (auto* window : hidden_windows_) {
      window->RemoveObserver(this);
      window->ClearProperty(kHideDuringWindowDragging);
    }
    hidden_windows_.clear();
  }

  void RestoreWindowsVisibility() {
    for (auto* window : hidden_windows_) {
      window->RemoveObserver(this);
      ScopedAnimationDisabler disabler(window);
      window->Show();
      window->ClearProperty(kHideDuringWindowDragging);
    }
    hidden_windows_.clear();
  }

  // Even though we explicitly minimize the windows, some (i.e. ARC apps)
  // minimize asynchronously so they may not be truly minimized after |this| is
  // constructed.
  bool WindowsMinimized() {
    return std::all_of(hidden_windows_.begin(), hidden_windows_.end(),
                       [](const aura::Window* w) {
                         return WindowState::Get(w)->IsMinimized();
                       });
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    hidden_windows_.erase(
        std::find(hidden_windows_.begin(), hidden_windows_.end(), window));
  }

 private:
  aura::Window* dragged_window_;
  std::vector<aura::Window*> hidden_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowsHider);
};

// static
float DragWindowFromShelfController::GetReturnToMaximizedThreshold() {
  return Shell::GetPrimaryRootWindowController()
      ->shelf()
      ->hotseat_widget()
      ->GetHotseatFullDragAmount();
}

DragWindowFromShelfController::DragWindowFromShelfController(
    aura::Window* window,
    const gfx::PointF& location_in_screen)
    : window_(window) {
  window_->AddObserver(this);
  OnDragStarted(location_in_screen);

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      window_->GetHost()->compositor(), kDragWindowFromShelfHistogram,
      kDragWindowFromShelfMaxLatencyHistogram);
}

DragWindowFromShelfController::~DragWindowFromShelfController() {
  CancelDrag();
  if (window_)
    window_->RemoveObserver(this);
}

void DragWindowFromShelfController::Drag(const gfx::PointF& location_in_screen,
                                         float scroll_x,
                                         float scroll_y) {
  // |window_| might have been destroyed during dragging.
  if (!window_)
    return;

  if (!drag_started_)
    return;

  presentation_time_recorder_->RequestNext();
  UpdateDraggedWindow(location_in_screen);

  // Open overview if the window has been dragged far enough and the scroll
  // delta has decreased to kOpenOverviewThreshold. Wait until all windows are
  // minimized or they will not show up in overview.
  DCHECK(windows_hider_);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (std::abs(scroll_y) <= kOpenOverviewThreshold &&
      !overview_controller->InOverviewSession() &&
      windows_hider_->WindowsMinimized()) {
    overview_controller->StartOverview(OverviewEnterExitType::kImmediateEnter);
    OnWindowDragStartedInOverview();
  }

  // If overview is active, update its splitview indicator during dragging if
  // splitview is allowed in current configuration.
  if (overview_controller->InOverviewSession()) {
    const SplitViewController::SnapPosition snap_position =
        GetSnapPosition(location_in_screen);
    const SplitViewDragIndicators::WindowDraggingState window_dragging_state =
        SplitViewDragIndicators::ComputeWindowDraggingState(
            /*is_dragging=*/true,
            SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            snap_position);
    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->UpdateSplitViewDragIndicatorsWindowDraggingStates(
        Shell::GetPrimaryRootWindow(), window_dragging_state);
    overview_session->OnWindowDragContinued(window_, location_in_screen,
                                            window_dragging_state);

    if (snap_position != SplitViewController::NONE) {
      // If the dragged window is in snap preview area, make sure overview is
      // visible.
      ShowOverviewDuringOrAfterDrag();
    } else if (std::abs(scroll_x) > kShowOverviewThreshold ||
               std::abs(scroll_y) > kShowOverviewThreshold) {
      // If the dragging velocity is large enough, hide overview windows.
      show_overview_timer_.Stop();
      HideOverviewDuringDrag();
    } else if (!show_overview_timer_.IsRunning() ||
               std::abs(scroll_x) > kScrollUpdateOverviewThreshold ||
               std::abs(scroll_y) > kScrollUpdateOverviewThreshold) {
      // Otherwise start the |show_overview_timer_| to show and update
      // overview when the dragging slows down or stops. Note if the window is
      // still being dragged with scroll rate more than
      // kScrollUpdateOverviewThreshold, we restart the show overview timer.
      show_overview_timer_.Start(
          FROM_HERE, kShowOverviewTimeWhenDragSuspend, this,
          &DragWindowFromShelfController::ShowOverviewDuringOrAfterDrag);
    }
  }

  previous_location_in_screen_ = location_in_screen;
}

base::Optional<ShelfWindowDragResult> DragWindowFromShelfController::EndDrag(
    const gfx::PointF& location_in_screen,
    base::Optional<float> velocity_y) {
  if (!drag_started_)
    return base::nullopt;

  UpdateDraggedWindow(location_in_screen);

  drag_started_ = false;
  previous_location_in_screen_ = location_in_screen;
  presentation_time_recorder_.reset();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool in_overview = overview_controller->InOverviewSession();
  const bool in_splitview = split_view_controller->InSplitViewMode();
  const bool drop_window_in_overview =
      ShouldDropWindowInOverview(location_in_screen, velocity_y);
  end_snap_position_ = GetSnapPositionOnDragEnd(location_in_screen, velocity_y);

  window_drag_result_ = base::nullopt;
  if (ShouldGoToHomeScreen(location_in_screen, velocity_y)) {
    DCHECK(!in_splitview);
    if (in_overview)
      overview_controller->EndOverview(OverviewEnterExitType::kFadeOutExit);
    window_drag_result_ = ShelfWindowDragResult::kGoToHomeScreen;
  } else if (ShouldRestoreToOriginalBounds(location_in_screen)) {
    window_drag_result_ = ShelfWindowDragResult::kRestoreToOriginalBounds;
  } else if (!in_overview) {
    // if overview is not active during the entire drag process, scale down the
    // dragged window to go to home screen.
    window_drag_result_ = ShelfWindowDragResult::kGoToHomeScreen;
  } else {
    if (drop_window_in_overview)
      window_drag_result_ = ShelfWindowDragResult::kGoToOverviewMode;
    else if (end_snap_position_ != SplitViewController::NONE)
      window_drag_result_ = ShelfWindowDragResult::kGoToSplitviewMode;
    // For window that may drop in overview or snap in split screen, restore its
    // original backdrop mode.
    WindowBackdrop::Get(window_)->RestoreBackdrop();
  }
  WindowState::Get(window_)->DeleteDragDetails();

  if (window_drag_result_.has_value()) {
    UMA_HISTOGRAM_ENUMERATION(kHandleDragWindowFromShelfHistogramName,
                              *window_drag_result_);
  }
  return window_drag_result_;
}

void DragWindowFromShelfController::CancelDrag() {
  if (!drag_started_)
    return;

  UMA_HISTOGRAM_ENUMERATION(kHandleDragWindowFromShelfHistogramName,
                            ShelfWindowDragResult::kDragCanceled);

  drag_started_ = false;
  presentation_time_recorder_.reset();
  // Reset the window's transform to identity transform.
  window_->SetTransform(gfx::Transform());
  WindowBackdrop::Get(window_)->RestoreBackdrop();

  // End overview if it was opened during dragging.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview(OverviewEnterExitType::kImmediateExit);
  ReshowHiddenWindowsOnDragEnd();

  window_drag_result_ = ShelfWindowDragResult::kDragCanceled;
  // When the drag is cancelled, the window should restore to its original snap
  // position.
  OnDragEnded(previous_location_in_screen_,
              /*should_drop_window_in_overview=*/false,
              /*snap_position=*/initial_snap_position_);
  WindowState::Get(window_)->DeleteDragDetails();
}

bool DragWindowFromShelfController::IsDraggedWindowAnimating() const {
  return window_ && window_->layer()->GetAnimator()->is_animating();
}

void DragWindowFromShelfController::FinalizeDraggedWindow() {
  if (!window_drag_result_.has_value()) {
    started_in_overview_ = false;
    return;
  }

  DCHECK(!drag_started_);
  DCHECK(window_);

  OnDragEnded(previous_location_in_screen_,
              *window_drag_result_ == ShelfWindowDragResult::kGoToOverviewMode,
              end_snap_position_);
}

void DragWindowFromShelfController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);

  CancelDrag();
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void DragWindowFromShelfController::AddObserver(
    DragWindowFromShelfController::Observer* observer) {
  observers_.AddObserver(observer);
}

void DragWindowFromShelfController::RemoveObserver(
    DragWindowFromShelfController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DragWindowFromShelfController::OnDragStarted(
    const gfx::PointF& location_in_screen) {
  drag_started_ = true;
  started_in_overview_ =
      Shell::Get()->overview_controller()->InOverviewSession();
  initial_location_in_screen_ = location_in_screen;
  previous_location_in_screen_ = location_in_screen;
  WindowState::Get(window_)->CreateDragDetails(
      initial_location_in_screen_, HTCLIENT, ::wm::WINDOW_MOVE_SOURCE_TOUCH);

  // Disable the backdrop on the dragged window during dragging.
  WindowBackdrop::Get(window_)->DisableBackdrop();

  // Hide all visible windows behind the dragged window during dragging.
  windows_hider_ = std::make_unique<WindowsHider>(window_);

  // Hide the home launcher until it's eligible to show it.
  Shell::Get()->home_screen_controller()->OnWindowDragStarted();

  // Use the same dim and blur as in overview during dragging.
  RootWindowController::ForWindow(window_->GetRootWindow())
      ->wallpaper_widget_controller()
      ->SetWallpaperProperty(wallpaper_constants::kOverviewInTabletState);

  // If the dragged window is one of the snapped window in splitview, it needs
  // to be detached from splitview before start dragging.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  // Preserve initial snap position
  if (split_view_controller->IsWindowInSplitView(window_)) {
    initial_snap_position_ =
        split_view_controller->GetPositionOfSnappedWindow(window_);
  }
  split_view_controller->OnWindowDragStarted(window_);
  // Note SplitViewController::OnWindowDragStarted() may open overview.
  if (Shell::Get()->overview_controller()->InOverviewSession())
    OnWindowDragStartedInOverview();
}

void DragWindowFromShelfController::OnDragEnded(
    const gfx::PointF& location_in_screen,
    bool should_drop_window_in_overview,
    SplitViewController::SnapPosition snap_position) {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    // Make sure overview is visible after drag ends.
    ShowOverviewDuringOrAfterDrag();

    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->ResetSplitViewDragIndicatorsWindowDraggingStates();
    overview_session->OnWindowDragEnded(
        window_, location_in_screen, should_drop_window_in_overview,
        /*snap=*/snap_position != SplitViewController::NONE);
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (split_view_controller->InSplitViewMode() ||
      snap_position != SplitViewController::NONE) {
    split_view_controller->OnWindowDragEnded(
        window_, snap_position, gfx::ToRoundedPoint(location_in_screen));
  }

  // Scale-in-to-show home screen if home screen should be shown after drag
  // ends.
  Shell::Get()->home_screen_controller()->OnWindowDragEnded(/*animate=*/true);

  // Clear the wallpaper dim and blur if not in overview after drag ends.
  // If in overview, the dim and blur will be cleared after overview ends.
  if (!overview_controller->InOverviewSession()) {
    RootWindowController::ForWindow(window_->GetRootWindow())
        ->wallpaper_widget_controller()
        ->SetWallpaperProperty(wallpaper_constants::kClear);
  }

  DCHECK(window_drag_result_.has_value());
  switch (*window_drag_result_) {
    case ShelfWindowDragResult::kGoToHomeScreen:
      ScaleDownWindowAfterDrag();
      break;
    case ShelfWindowDragResult::kRestoreToOriginalBounds:
      ScaleUpToRestoreWindowAfterDrag();
      break;
    case ShelfWindowDragResult::kGoToOverviewMode:
    case ShelfWindowDragResult::kGoToSplitviewMode:
    case ShelfWindowDragResult::kDragCanceled:
      // No action is needed.
      break;
  }
  window_drag_result_.reset();
  started_in_overview_ = false;
}

void DragWindowFromShelfController::UpdateDraggedWindow(
    const gfx::PointF& location_in_screen) {
  gfx::Rect bounds = window_->bounds();
  ::wm::ConvertRectToScreen(window_->parent(), &bounds);

  // Calculate the window's transform based on the location.
  // For scale, at |initial_location_in_screen_| or bounds.bottom(), the scale
  // is 1.0, and at the |min_y| position of its bounds, it reaches to its
  // minimum scale |kMinimumWindowScaleDuringDragging|. Calculate the desired
  // scale based on the current y position.
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(gfx::ToRoundedPoint(location_in_screen))
          .bounds();
  const float min_y = display_bounds.y() +
                      display_bounds.height() * kMinYDisplayHeightRatio +
                      kMinimumWindowScaleDuringDragging * bounds.height();
  float y_full =
      std::min(initial_location_in_screen_.y(), (float)bounds.bottom()) - min_y;
  float y_diff = location_in_screen.y() - min_y;
  float scale = (1.0f - kMinimumWindowScaleDuringDragging) * y_diff / y_full +
                kMinimumWindowScaleDuringDragging;
  scale = base::ClampToRange(scale, /*min=*/kMinimumWindowScaleDuringDragging,
                             /*max=*/1.f);

  // Calculate the desired translation so that the dragged window stays under
  // the finger during the dragging.
  // Since vertical drag doesn't start until after passing the top of the shelf,
  // the y calculations should be relative to the window bounds instead of
  // |initial_location_in_screen| (which is on the shelf)
  gfx::Transform transform;
  transform.Translate(
      (location_in_screen.x() - bounds.x()) -
          (initial_location_in_screen_.x() - bounds.x()) * scale,
      (location_in_screen.y() - bounds.y()) - bounds.height() * scale);
  transform.Scale(scale, scale);

  // The dragged window cannot exceed the top or bottom of the display. So
  // calculate the expected transformed bounds and then adjust the transform if
  // needed.
  gfx::RectF transformed_bounds(window_->bounds());
  gfx::Transform new_tranform = TransformAboutPivot(
      gfx::ToRoundedPoint(transformed_bounds.origin()), transform);
  new_tranform.TransformRect(&transformed_bounds);
  ::wm::TranslateRectToScreen(window_->parent(), &transformed_bounds);
  if (transformed_bounds.y() < display_bounds.y()) {
    transform.Translate(0,
                        (display_bounds.y() - transformed_bounds.y()) / scale);
  } else if (transformed_bounds.bottom() > bounds.bottom()) {
    DCHECK_EQ(1.f, scale);
    transform.Translate(
        0, (bounds.bottom() - transformed_bounds.bottom()) / scale);
  }

  SetTransform(window_, transform);
}

SplitViewController::SnapPosition
DragWindowFromShelfController::GetSnapPosition(
    const gfx::PointF& location_in_screen) const {
  // if |location_in_screen| is close to the bottom of the screen and is
  // inside of GetReturnToMaximizedThreshold() threshold, we should not try to
  // snap the window.
  if (ShouldRestoreToOriginalBounds(location_in_screen))
    return SplitViewController::NONE;

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  SplitViewController::SnapPosition snap_position = ::ash::GetSnapPosition(
      root_window, window_, gfx::ToRoundedPoint(location_in_screen),
      gfx::ToRoundedPoint(initial_location_in_screen_),
      /*snap_distance_from_edge=*/kDistanceFromEdge,
      /*minimum_drag_distance=*/kMinDragDistance,
      /*horizontal_edge_inset=*/kScreenEdgeInsetForSnap,
      /*vertical_edge_inset=*/kScreenEdgeInsetForSnap);

  // For portrait mode, since the drag starts from the bottom of the screen,
  // we should only allow the window to snap to the top of the screen.
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const bool is_primary = IsCurrentScreenOrientationPrimary();
  if (!is_landscape &&
      ((is_primary && snap_position == SplitViewController::RIGHT) ||
       (!is_primary && snap_position == SplitViewController::LEFT))) {
    snap_position = SplitViewController::NONE;
  }

  return snap_position;
}

bool DragWindowFromShelfController::ShouldRestoreToOriginalBounds(
    const gfx::PointF& location_in_screen) const {
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(gfx::ToRoundedPoint(location_in_screen))
          .bounds();
  gfx::RectF transformed_window_bounds =
      window_util::GetTransformedBounds(window_, /*top_inset=*/0);

  return transformed_window_bounds.bottom() >
         display_bounds.bottom() - GetReturnToMaximizedThreshold();
}

bool DragWindowFromShelfController::ShouldGoToHomeScreen(
    const gfx::PointF& location_in_screen,
    base::Optional<float> velocity_y) const {
  // If the drag ends below the shelf, do not go to home screen (theoretically
  // it may happen in kExtended hotseat case when drag can start and end below
  // the shelf).
  if (location_in_screen.y() >=
      Shelf::ForWindow(window_)->GetIdealBoundsForWorkAreaCalculation().y()) {
    return false;
  }

  // Do not go home if we're in split screen.
  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InSplitViewMode()) {
    return false;
  }

  // If overview is invisible when the drag ends, no matter what the velocity
  // is, we should go to home screen.
  if (Shell::Get()->overview_controller()->InOverviewSession() &&
      !show_overview_windows_) {
    return true;
  }

  // Otherwise go home if the velocity is large enough.
  return velocity_y.has_value() && *velocity_y < 0 &&
         std::abs(*velocity_y) >= kVelocityToHomeScreenThreshold;
}

SplitViewController::SnapPosition
DragWindowFromShelfController::GetSnapPositionOnDragEnd(
    const gfx::PointF& location_in_screen,
    base::Optional<float> velocity_y) const {
  if (!Shell::Get()->overview_controller()->InOverviewSession() ||
      ShouldGoToHomeScreen(location_in_screen, velocity_y)) {
    return SplitViewController::NONE;
  }

  // When dragging ends but restore to original bounds, we should restore
  // window's initial snap position
  if (ShouldRestoreToOriginalBounds(location_in_screen))
    return initial_snap_position_;

  return GetSnapPosition(location_in_screen);
}

bool DragWindowFromShelfController::ShouldDropWindowInOverview(
    const gfx::PointF& location_in_screen,
    base::Optional<float> velocity_y) const {
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return false;

  if (ShouldGoToHomeScreen(location_in_screen, velocity_y))
    return false;

  const bool in_splitview =
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InSplitViewMode();
  if (!in_splitview && ShouldRestoreToOriginalBounds(location_in_screen)) {
    return false;
  }

  if (in_splitview) {
    if (velocity_y.has_value() && *velocity_y < 0 &&
        std::abs(*velocity_y) >= kVelocityToOverviewThreshold) {
      return true;
    }
    if (ShouldRestoreToOriginalBounds(location_in_screen))
      return false;
  }

  return GetSnapPositionOnDragEnd(location_in_screen, velocity_y) ==
         SplitViewController::NONE;
}

void DragWindowFromShelfController::ReshowHiddenWindowsOnDragEnd() {
  windows_hider_->RestoreWindowsVisibility();
}

void DragWindowFromShelfController::ShowOverviewDuringOrAfterDrag() {
  show_overview_timer_.Stop();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return;

  show_overview_windows_ = true;
  overview_controller->overview_session()->SetVisibleDuringWindowDragging(
      /*visible=*/true, /*animate=*/true);
  for (Observer& observer : observers_)
    observer.OnOverviewVisibilityChanged(true);
}

void DragWindowFromShelfController::HideOverviewDuringDrag() {
  show_overview_windows_ = false;

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return;
  overview_controller->overview_session()->SetVisibleDuringWindowDragging(
      /*visible=*/false,
      /*animate=*/false);
  for (Observer& observer : observers_)
    observer.OnOverviewVisibilityChanged(false);
}

void DragWindowFromShelfController::ScaleDownWindowAfterDrag() {
  // Notify home screen controller that the home screen is about to be shown, so
  // home screen and shelf start updating their state as the window is
  // minimizing.
  Shell::Get()
      ->home_screen_controller()
      ->delegate()
      ->OnHomeLauncherPositionChanged(
          /*percent_shown=*/100,
          display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Do the scale-down transform for the entire transient tree.
  for (auto* window : GetTransientTreeIterator(window_)) {
    // self-destructed when window transform animation is done.
    new WindowScaleAnimation(
        window, WindowScaleAnimation::WindowScaleType::kScaleDownToShelf,
        window == window_
            ? base::BindOnce(
                  &DragWindowFromShelfController::OnWindowScaledDownAfterDrag,
                  weak_ptr_factory_.GetWeakPtr())
            : base::NullCallback());
  }
}

void DragWindowFromShelfController::OnWindowScaledDownAfterDrag() {
  HomeScreenController* home_screen_controller =
      Shell::Get()->home_screen_controller();
  if (!home_screen_controller || !home_screen_controller->delegate())
    return;

  home_screen_controller->delegate()->OnHomeLauncherAnimationComplete(
      /*shown=*/true, display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void DragWindowFromShelfController::ScaleUpToRestoreWindowAfterDrag() {
  // Do the scale up transform for the entire transient tee.
  for (auto* window : GetTransientTreeIterator(window_)) {
    new WindowScaleAnimation(
        window, WindowScaleAnimation::WindowScaleType::kScaleUpToRestore,
        base::BindOnce(
            &DragWindowFromShelfController::OnWindowRestoredToOrignalBounds,
            weak_ptr_factory_.GetWeakPtr(),
            /*should_end_overview=*/!started_in_overview_));
  }
}

void DragWindowFromShelfController::OnWindowRestoredToOrignalBounds(
    bool end_overview) {
  base::AutoReset<bool> auto_reset(&during_window_restoration_callback_, true);
  if (end_overview) {
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEnterExitType::kImmediateExit);
  }
  ReshowHiddenWindowsOnDragEnd();
}

void DragWindowFromShelfController::OnWindowDragStartedInOverview() {
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DCHECK(overview_session);
  overview_session->OnWindowDragStarted(window_, /*animate=*/false);
  if (ShouldAllowSplitView())
    overview_session->SetSplitViewDragIndicatorsDraggedWindow(window_);
  // Hide overview windows first and fade in the windows after delaying
  // kShowOverviewTimeWhenDragSuspend.
  HideOverviewDuringDrag();
}

}  // namespace ash
