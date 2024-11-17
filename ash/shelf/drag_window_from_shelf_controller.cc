// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_window_from_shelf_controller.h"

#include <algorithm>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/window_scale_animation.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
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
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/scoped_animation_disabler.h"
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
    base::Milliseconds(40);

// The scroll update threshold to restart the show overview timer.
constexpr float kScrollUpdateOverviewThreshold = 2.f;

// Once we have dragged the window more than the display height divided by this
// ratio, the other window copy will be fully faded out.
constexpr float kOtherWindowFullFadeHeightRatio = 8.f;

// The other window will be scaled down to this maximum during dragging.
constexpr float kOtherWindowMaxScale = 0.9f;

// Presentation time histogram names.
constexpr char kDragWindowFromShelfHistogram[] =
    "Ash.DragWindowFromShelf.PresentationTime";
constexpr char kDragWindowFromShelfMaxLatencyHistogram[] =
    "Ash.DragWindowFromShelf.PresentationTime.MaxLatency";

// Self deleting class that takes ownership of the other window copy and
// animates it on drag finished. Deletes itself when the animation is done. The
// other window refers to the secondary window that moves when dragging from
// the shelf.
class OtherWindowCopyAnimation {
 public:
  // Takes ownership of the layer tree `other_winodw_copy`. Use a fade in and
  // scale up animation if `show` is true. Use a fade out animation if `show` is
  // false.
  OtherWindowCopyAnimation(
      std::unique_ptr<ui::LayerTreeOwner> other_window_copy,
      bool show)
      : other_window_copy_(std::move(other_window_copy)) {
    ui::Layer* layer = other_window_copy_->root();

    views::AnimationBuilder builder;
    builder
        .OnEnded(base::BindOnce(&OtherWindowCopyAnimation::OnAnimationEnded,
                                base::Unretained(this)))
        .OnAborted(base::BindOnce(&OtherWindowCopyAnimation::OnAnimationEnded,
                                  base::Unretained(this)))
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(base::Milliseconds(350))
        .SetOpacity(layer, show ? 1.f : 0.f, gfx::Tween::LINEAR);
    if (show) {
      builder.Once()
          .SetDuration(base::Milliseconds(350))
          .SetTransform(layer, gfx::Transform(), gfx::Tween::LINEAR);
    }
  }
  OtherWindowCopyAnimation(const OtherWindowCopyAnimation&) = delete;
  OtherWindowCopyAnimation& operator=(const OtherWindowCopyAnimation&) = delete;
  ~OtherWindowCopyAnimation() = default;

  void OnAnimationEnded() { delete this; }

 private:
  std::unique_ptr<ui::LayerTreeOwner> other_window_copy_;
};

}  // namespace

// Hide all visible windows expect the dragged windows or the window showing in
// splitview during dragging.
class DragWindowFromShelfController::WindowsHider
    : public aura::WindowObserver {
 public:
  WindowsHider(aura::Window* dragged_window, aura::Window* other_window)
      : dragged_window_(dragged_window) {
    std::vector<raw_ptr<aura::Window, VectorExperimental>> windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
    for (aura::Window* window : windows) {
      if (window == dragged_window_ || window == other_window) {
        continue;
      }
      if (wm::HasTransientAncestor(window, dragged_window_))
        continue;
      if (!window->IsVisible())
        continue;
      if (SplitViewController::Get(window)->IsWindowInSplitView(window))
        continue;
      auto* overview_controller = Shell::Get()->overview_controller();
      if (overview_controller->InOverviewSession() &&
          overview_controller->overview_session()->IsWindowInOverview(window)) {
        continue;
      }

      hidden_windows_.push_back(window);
      window->AddObserver(this);
      window->SetProperty(kHideDuringWindowDragging, true);
    }
    window_util::MinimizeAndHideWithoutAnimation(hidden_windows_);
  }

  WindowsHider(const WindowsHider&) = delete;
  WindowsHider& operator=(const WindowsHider&) = delete;

  ~WindowsHider() override {
    for (aura::Window* window : hidden_windows_) {
      window->RemoveObserver(this);
      window->ClearProperty(kHideDuringWindowDragging);
    }
    hidden_windows_.clear();
  }

  void RestoreWindowsVisibility() {
    for (aura::Window* window : hidden_windows_) {
      window->RemoveObserver(this);
      wm::ScopedAnimationDisabler disabler(window);
      window->Show();
      window->ClearProperty(kHideDuringWindowDragging);
    }
    hidden_windows_.clear();
  }

  // Even though we explicitly minimize the windows, some (i.e. ARC apps)
  // minimize asynchronously so they may not be truly minimized after |this| is
  // constructed.
  bool WindowsMinimized() {
    return base::ranges::all_of(hidden_windows_, [](const aura::Window* w) {
      return WindowState::Get(w)->IsMinimized();
    });
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    hidden_windows_.erase(base::ranges::find(hidden_windows_, window));
  }

 private:
  raw_ptr<aura::Window, DanglingUntriaged> dragged_window_;
  std::vector<raw_ptr<aura::Window, VectorExperimental>> hidden_windows_;
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

  // Find the other window that is visible while `window_` is being dragged.
  // There will only be another window if there is a float window (splitview has
  // two visible windows but is handled separately).
  if (auto* floated_window = window_util::GetFloatedWindowForActiveDesk()) {
    // If the floated window is the dragged window, then the other window is
    // the top most non floated window, if it exists. Otherwise the floated
    // window is the active window.
    if (floated_window == window_) {
      aura::Window* candidate_other_window =
          window_util::GetTopNonFloatedWindow();
      if (candidate_other_window &&
          !WindowState::Get(candidate_other_window)->IsMinimized()) {
        other_window_ = candidate_other_window;
      }
    } else {
      other_window_ = floated_window;
    }

    // Create a copy of the other window. This will be stacked on top and
    // faded out as we drag. The original window will be placed immediately
    // into overview mode on a successful drag, or return to its original
    // position on a canceled drag.
    if (other_window_) {
      other_window_->AddObserver(this);
      other_window_copy_ = wm::RecreateLayers(other_window_);
      other_window_copy_->root()->SetVisible(true);
      other_window_copy_->root()->SetOpacity(1.f);

      // If `other_window_` is the floated window, we need to move the copy to
      // the active desk container. The float container will be moved under the
      // desk containers (see `ScopedFloatContainerStacker `), so that the
      // overview item does not appear above the dragged window during the drag.
      if (other_window_ == floated_window) {
        ui::Layer* new_parent = desks_util::GetActiveDeskContainerForRoot(
                                    Shell::GetPrimaryRootWindow())
                                    ->layer();
        new_parent->Add(other_window_copy_->root());
      } else {
        other_window_->layer()->parent()->StackAbove(other_window_copy_->root(),
                                                     other_window_->layer());
      }
    }
  }

  OnDragStarted(location_in_screen);

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      window_->GetHost()->compositor(), kDragWindowFromShelfHistogram,
      kDragWindowFromShelfMaxLatencyHistogram);
}

DragWindowFromShelfController::~DragWindowFromShelfController() {
  CancelDrag();
  if (window_)
    window_->RemoveObserver(this);
  ResetOtherWindow(/*show=*/std::nullopt);
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

  DCHECK(windows_hider_);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (std::abs(scroll_y) <= kOpenOverviewThreshold &&
      windows_hider_->WindowsMinimized()) {
    // Open overview if the window has been dragged far enough and the scroll
    // delta has decreased to `kOpenOverviewThreshold`. Wait until all windows
    // are minimized or they will not show up in overview.
    if (!overview_controller->InOverviewSession() &&
        overview_controller->StartOverview(
            OverviewStartAction::kDragWindowFromShelf,
            OverviewEnterExitType::kImmediateEnter)) {
      OnWindowDragStartedInOverview();
    }
  }

  // If overview is active, update its splitview indicator during dragging if
  // splitview is allowed in current configuration.
  if (overview_controller->InOverviewSession()) {
    const SnapPosition snap_position = GetSnapPosition(location_in_screen);
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

    if (snap_position != SnapPosition::kNone) {
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

std::optional<ShelfWindowDragResult> DragWindowFromShelfController::EndDrag(
    const gfx::PointF& location_in_screen,
    std::optional<float> velocity_y) {
  if (!drag_started_)
    return std::nullopt;

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

  window_drag_result_ = std::nullopt;
  if (ShouldGoToHomeScreen(location_in_screen, velocity_y)) {
    DCHECK(!in_splitview);
    if (in_overview) {
      overview_controller->EndOverview(OverviewEndAction::kDragWindowFromShelf,
                                       OverviewEnterExitType::kFadeOutExit);
    }
    window_drag_result_ = ShelfWindowDragResult::kGoToHomeScreen;
  } else if (ShouldRestoreToOriginalBounds(location_in_screen, velocity_y)) {
    window_drag_result_ = ShelfWindowDragResult::kRestoreToOriginalBounds;
  } else if (!in_overview) {
    // if overview is not active during the entire drag process, scale down the
    // dragged window to go to home screen.
    window_drag_result_ = ShelfWindowDragResult::kGoToHomeScreen;
  } else {
    if (drop_window_in_overview) {
      window_drag_result_ = ShelfWindowDragResult::kGoToOverviewMode;
    } else if (end_snap_position_ != SnapPosition::kNone) {
      window_drag_result_ = ShelfWindowDragResult::kGoToSplitviewMode;
    }
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
  if (overview_controller->InOverviewSession()) {
    overview_controller->EndOverview(OverviewEndAction::kDragWindowFromShelf,
                                     OverviewEnterExitType::kImmediateExit);
  }
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
  if (window == other_window_) {
    ResetOtherWindow(/*show=*/std::nullopt);
    return;
  }

  DCHECK_EQ(window_, window);

  CancelDrag();
  window_->RemoveObserver(this);
  window_ = nullptr;
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
  windows_hider_ = std::make_unique<WindowsHider>(window_, other_window_);

  // Hide the home launcher until it's eligible to show it.
  Shell::Get()->app_list_controller()->OnWindowDragStarted();

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
    SnapPosition snap_position) {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    // Make sure overview is visible after drag ends.
    ShowOverviewDuringOrAfterDrag();

    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->ResetSplitViewDragIndicatorsWindowDraggingStates();

    // No need to reposition overview windows if we are not dropping the dragged
    // window into overview. Overview will either be exited or unchanged, and
    // the extra movement from existing window will just add unnecessary
    // movement which will also slow down our dragged window animation.
    if (!should_drop_window_in_overview)
      overview_session->SuspendReposition();
    overview_session->OnWindowDragEnded(
        window_, location_in_screen, should_drop_window_in_overview,
        /*snap=*/snap_position != SnapPosition::kNone);
    overview_session->ResumeReposition();
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (split_view_controller->InSplitViewMode() ||
      snap_position != SnapPosition::kNone) {
    split_view_controller->OnWindowDragEnded(
        window_, snap_position, gfx::ToRoundedPoint(location_in_screen),
        WindowSnapActionSource::kDragUpFromShelfToSnap);
  }

  // Scale-in-to-show home screen if home screen should be shown after drag
  // ends.
  Shell::Get()->app_list_controller()->OnWindowDragEnded(/*animate=*/true);

  DCHECK(window_drag_result_.has_value());
  switch (*window_drag_result_) {
    case ShelfWindowDragResult::kGoToHomeScreen:
      ScaleDownWindowAfterDrag();
      windows_hider_.reset();
      break;
    case ShelfWindowDragResult::kRestoreToOriginalBounds:
      ScaleUpToRestoreWindowAfterDrag();
      // Do not reset |windows_hider_| here because
      // |ScaleUpToRestoreWindowAfterDrag()| ends up using |windows_hider_| in
      // an async manner.
      break;
    case ShelfWindowDragResult::kGoToOverviewMode:
    case ShelfWindowDragResult::kGoToSplitviewMode:
    case ShelfWindowDragResult::kDragCanceled:
      windows_hider_.reset();
      break;
  }

  // If it exists, `other_window_copy_` will restore to its initial bounds and
  // opacity if the drag result is to restore windows to their original bounds.
  // Otherwise we fade out the copy before destroying it.
  ResetOtherWindow(/*show=*/*window_drag_result_ ==
                   ShelfWindowDragResult::kRestoreToOriginalBounds);

  window_drag_result_.reset();
  started_in_overview_ = false;
}

void DragWindowFromShelfController::UpdateDraggedWindow(
    const gfx::PointF& location_in_screen) {
  gfx::Rect bounds = window_->bounds();
  wm::ConvertRectToScreen(window_->parent(), &bounds);

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
  scale = std::clamp(scale, /*min=*/kMinimumWindowScaleDuringDragging,
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
  const gfx::Transform new_tranform =
      TransformAboutPivot(gfx::PointF(window_->bounds().origin()), transform);
  gfx::RectF transformed_bounds =
      new_tranform.MapRect(gfx::RectF(window_->bounds()));
  wm::TranslateRectToScreen(window_->parent(), &transformed_bounds);
  if (transformed_bounds.y() < display_bounds.y()) {
    transform.Translate(0,
                        (display_bounds.y() - transformed_bounds.y()) / scale);
  } else if (transformed_bounds.bottom() > bounds.bottom()) {
    DCHECK_EQ(1.f, scale);
    transform.Translate(
        0, (bounds.bottom() - transformed_bounds.bottom()) / scale);
  }

  window_util::SetTransform(window_, transform);

  if (other_window_copy_) {
    // When we have dragged 1/8th of the display height, the copy should be
    // fully faded out and shrunk.
    float copy_scale =
        (bounds.bottom() - location_in_screen.y()) /
        (display_bounds.height() / kOtherWindowFullFadeHeightRatio);
    copy_scale = 1.f - std::clamp(copy_scale, 0.f, 1.f);

    other_window_copy_->root()->SetOpacity(copy_scale);

    CHECK(other_window_);
    if (!WindowState::Get(other_window_)->IsFloated()) {
      const float copy_transform_scale =
          std::clamp(copy_scale, kOtherWindowMaxScale, 1.f);
      const gfx::Transform copy_transform = gfx::GetScaleTransform(
          other_window_copy_->root()->bounds().CenterPoint(),
          copy_transform_scale);
      other_window_copy_->root()->SetTransform(copy_transform);
    }
  }
}

SnapPosition DragWindowFromShelfController::GetSnapPosition(
    const gfx::PointF& location_in_screen) const {
  // if |location_in_screen| is close to the bottom of the screen and is
  // inside of GetReturnToMaximizedThreshold() threshold, we should not try to
  // snap the window.
  if (ShouldRestoreToOriginalBounds(location_in_screen, std::nullopt)) {
    return SnapPosition::kNone;
  }

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  SnapPosition snap_position = ::ash::GetSnapPosition(
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
      ((is_primary && snap_position == SnapPosition::kSecondary) ||
       (!is_primary && snap_position == SnapPosition::kPrimary))) {
    snap_position = SnapPosition::kNone;
  }

  return snap_position;
}

bool DragWindowFromShelfController::ShouldRestoreToOriginalBounds(
    const gfx::PointF& location_in_screen,
    std::optional<float> velocity_y) const {
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(gfx::ToRoundedPoint(location_in_screen))
          .bounds();
  gfx::RectF transformed_window_bounds =
      window_util::GetTransformedBounds(window_, /*top_inset=*/0);

  // If overview is invisible when the drag ends with downward velocity, we
  // should restore to original bounds.
  if (Shell::Get()->overview_controller()->InOverviewSession() &&
      !show_overview_windows_ && velocity_y.has_value() &&
      velocity_y.value() > 0) {
    return true;
  }

  // Otherwise restore the bounds if the downward vertical velocity exceeds the
  // threshold, or if the bottom of the dragged window is within the
  // GetReturnToMaximizedThreshold() threshold.
  return (velocity_y.has_value() &&
          velocity_y.value() >= kVelocityToRestoreBoundsThreshold) ||
         transformed_window_bounds.bottom() >
             display_bounds.bottom() - GetReturnToMaximizedThreshold();
}

bool DragWindowFromShelfController::ShouldGoToHomeScreen(
    const gfx::PointF& location_in_screen,
    std::optional<float> velocity_y) const {
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

  // If overview is invisible when the drag ends with upward velocity or no
  // velocity, we should go to home screen.
  if (Shell::Get()->overview_controller()->InOverviewSession() &&
      !show_overview_windows_ &&
      (!velocity_y.has_value() || velocity_y.value() <= 0)) {
    return true;
  }

  // Otherwise go home if the upward vertical velocity exceeds the threshold.
  return velocity_y.has_value() &&
         velocity_y.value() <= -kVelocityToHomeScreenThreshold;
}

SnapPosition DragWindowFromShelfController::GetSnapPositionOnDragEnd(
    const gfx::PointF& location_in_screen,
    std::optional<float> velocity_y) const {
  if (!Shell::Get()->overview_controller()->InOverviewSession() ||
      ShouldGoToHomeScreen(location_in_screen, velocity_y)) {
    return SnapPosition::kNone;
  }

  // When dragging ends but restore to original bounds, we should restore
  // window's initial snap position
  if (ShouldRestoreToOriginalBounds(location_in_screen, velocity_y))
    return initial_snap_position_;

  return GetSnapPosition(location_in_screen);
}

bool DragWindowFromShelfController::ShouldDropWindowInOverview(
    const gfx::PointF& location_in_screen,
    std::optional<float> velocity_y) const {
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return false;

  if (ShouldGoToHomeScreen(location_in_screen, velocity_y))
    return false;

  const bool in_splitview =
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InSplitViewMode();
  if (!in_splitview &&
      ShouldRestoreToOriginalBounds(location_in_screen, velocity_y)) {
    return false;
  }

  if (in_splitview) {
    if (velocity_y.has_value() &&
        velocity_y.value() <= -kVelocityToOverviewThreshold) {
      return true;
    }
    if (ShouldRestoreToOriginalBounds(location_in_screen, velocity_y))
      return false;
  }

  return GetSnapPositionOnDragEnd(location_in_screen, velocity_y) ==
         SnapPosition::kNone;
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
  if (on_overview_shown_callback_for_testing_)
    std::move(on_overview_shown_callback_for_testing_).Run();
}

void DragWindowFromShelfController::HideOverviewDuringDrag() {
  show_overview_windows_ = false;

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return;
  overview_controller->overview_session()->SetVisibleDuringWindowDragging(
      /*visible=*/false,
      /*animate=*/false);
}

void DragWindowFromShelfController::ScaleDownWindowAfterDrag() {
  // Notify home screen controller that the home screen is about to be shown, so
  // home screen and shelf start updating their state as the window is
  // minimizing.
  Shell::Get()->app_list_controller()->OnHomeLauncherPositionChanged(
      /*percent_shown=*/100,
      display::Screen::GetScreen()->GetPrimaryDisplay().id());

  (new WindowScaleAnimation(
       WindowScaleAnimation::WindowScaleType::kScaleDownToShelf,
       base::BindOnce(
           &DragWindowFromShelfController::OnWindowScaledDownAfterDrag,
           weak_ptr_factory_.GetWeakPtr())))
      ->Start(window_);
}

void DragWindowFromShelfController::OnWindowScaledDownAfterDrag() {
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  if (!app_list_controller)
    return;

  app_list_controller->OnHomeLauncherAnimationComplete(
      /*shown=*/true, display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void DragWindowFromShelfController::ScaleUpToRestoreWindowAfterDrag() {
  (new WindowScaleAnimation(
       WindowScaleAnimation::WindowScaleType::kScaleUpToRestore,
       base::BindOnce(
           &DragWindowFromShelfController::OnWindowRestoredToOriginalBounds,
           weak_ptr_factory_.GetWeakPtr(),
           /*should_end_overview=*/!started_in_overview_)))
      ->Start(window_);
}

void DragWindowFromShelfController::OnWindowRestoredToOriginalBounds(
    bool end_overview) {
  base::AutoReset<bool> auto_reset(&during_window_restoration_, true);
  // If `last_overview_drag_session_ptr_` is null, that means another party
  // started an overview session between the time the drag finished and the
  // `WindowScaleAnimation` was able to restore the window to the original
  // bounds. Don't end overview in this case since doing so would disrupt the
  // latest overview activity.
  if (end_overview && last_overview_drag_session_ptr_) {
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kDragWindowFromShelf,
        OverviewEnterExitType::kImmediateExit);
  }
  ReshowHiddenWindowsOnDragEnd();
}

void DragWindowFromShelfController::OnWindowDragStartedInOverview() {
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DCHECK(overview_session);
  last_overview_drag_session_ptr_ = overview_session->GetWeakPtr();
  overview_session->OnWindowDragStarted(window_, /*animate=*/false);
  if (ShouldAllowSplitView())
    overview_session->SetSplitViewDragIndicatorsDraggedWindow(window_);
  // Hide overview windows first and fade in the windows after delaying
  // kShowOverviewTimeWhenDragSuspend.
  HideOverviewDuringDrag();
}

void DragWindowFromShelfController::ResetOtherWindow(std::optional<bool> show) {
  if (other_window_) {
    other_window_->RemoveObserver(this);
    other_window_ = nullptr;

    if (show.has_value()) {
      DCHECK(other_window_copy_);

      // We can skip the animation if the copy is already dragged to its final
      // opacity and transform (if showing). This can happen since the copy is
      // fully transparent after dragging more than 1/8th of the display height.
      ui::Layer* layer = other_window_copy_->root();
      if (show.value()) {
        if (layer->GetTargetOpacity() != 1.f &&
            layer->GetTargetTransform() != gfx::Transform()) {
          new OtherWindowCopyAnimation(std::move(other_window_copy_),
                                       /*show=*/true);
        }
      } else {
        if (layer->GetTargetOpacity() != 0.f) {
          new OtherWindowCopyAnimation(std::move(other_window_copy_),
                                       /*show=*/false);
        }
      }
    }
  }
  other_window_copy_.reset();
}

}  // namespace ash
