// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_wm_metrics.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_observer.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace arc {

namespace {

// Histogram of the delay for window maximizing operation.
constexpr char kWindowMaximizedTimeHistogramPrefix[] =
    "Arc.WM.WindowMaximizedDelayTimeV2.";
// Histogram of the delay for window minimizing operation.
constexpr char kWindowMinimizedTimeHistogramPrefix[] =
    "Arc.WM.WindowMinimizedDelayTime.";
// Histogram of the delay for window closing operation.
constexpr char kWindowClosedTimeHistogramPrefix[] =
    "Arc.WM.WindowClosedDelayTimeV2.";
// Histogram of the delay for window state transition when entering into tablet
// mode.
constexpr char kWindowEnterTabletModeTimeHistogramPrefix[] =
    "Arc.WM.WindowEnterTabletModeDelayTimeV2.";
// Histogram of the delay for window state transition when exiting tablet mode.
constexpr char kWindowExitTabletModeTimeHistogramPrefix[] =
    "Arc.WM.WindowExitTabletModeDelayTimeV2.";
// Histogram of the delay for window bounds change when display rotates in
// tablet mode.
constexpr char kWindowRotateTimeHistogramPrefix[] =
    "Arc.WM.WindowRotateDelayTime.";

constexpr char kArcHistogramName[] = "ArcApp";
constexpr char kBrowserHistogramName[] = "Browser";
constexpr char kChromeAppHistogramName[] = "ChromeApp";
constexpr char kSystemAppHistogramName[] = "SystemApp";
constexpr char kCrostiniAppHistogramName[] = "CrostiniApp";

std::string GetAppTypeName(chromeos::AppType app_type) {
  switch (app_type) {
    case chromeos::AppType::ARC_APP:
      return kArcHistogramName;
    case chromeos::AppType::BROWSER:
      return kBrowserHistogramName;
    case chromeos::AppType::CHROME_APP:
      return kChromeAppHistogramName;
    case chromeos::AppType::SYSTEM_APP:
      return kSystemAppHistogramName;
    case chromeos::AppType::CROSTINI_APP:
      return kCrostiniAppHistogramName;
    default:
      return "Others";
  }
}

}  // namespace

// A window state observer that records the delay of window operation (e.g.,
// maximizing and minimizing).
class ArcWmMetrics::WindowStateChangeObserver
    : public ash::WindowStateObserver {
 public:
  WindowStateChangeObserver(aura::Window* window,
                            ui::mojom::WindowShowState old_window_show_state,
                            base::OnceClosure callback)
      : window_(window),
        old_window_show_state_(old_window_show_state),
        window_operation_completed_callback_(std::move(callback)) {
    auto* window_state = ash::WindowState::Get(window);
    CHECK(window_state);
    window_state_observation_.Observe(window_state);
  }

  WindowStateChangeObserver(const WindowStateChangeObserver&) = delete;
  WindowStateChangeObserver& operator=(const WindowStateChangeObserver) =
      delete;
  ~WindowStateChangeObserver() override = default;

  // ash::WindowStateObserver:
  void OnPostWindowStateTypeChange(
      ash::WindowState* new_window_state,
      chromeos::WindowStateType old_window_state_type) override {
    // For non-client-controlled windows, if the window remain maximized after
    // leaving tablet mode, `OnPostWindowStateTypeChange` is called with both
    // old state type and new state type equal to `kMaximized`. The histogram
    // does not record data in this case.
    if (old_window_state_type != new_window_state->GetStateType() &&
        old_window_state_type ==
            chromeos::ToWindowStateType(old_window_show_state_)) {
      RecordWindowStateChangeDelay(new_window_state);
    }

    std::move(window_operation_completed_callback_).Run();
  }

 private:
  void RecordWindowStateChangeDelay(ash::WindowState* state) {
    const chromeos::AppType app_type =
        window_->GetProperty(chromeos::kAppTypeKey);
    if (display::Screen::GetScreen()->InTabletMode()) {
      // When entering tablet mode, we only collect the data of visible window.
      if (state->IsMaximized() && window_->IsVisible()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(5), 100);
      }
    } else {
      if (state->IsMaximized()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowMaximizedTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(3), 100);
      } else if (state->IsMinimized()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowMinimizedTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(2), 100);
      } else if (state->IsNormalStateType()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowExitTabletModeTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(5), 100);
      }
    }
  }

  const raw_ptr<aura::Window> window_;
  const ui::mojom::WindowShowState old_window_show_state_;

  // Tracks the elapsed time from the window operation happens until the window
  // state is changed.
  base::ElapsedTimer window_operation_elapsed_timer_;

  base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
      window_state_observation_{this};

  base::OnceClosure window_operation_completed_callback_;
};

// A window observer that records the delay of window closing operation for ARC
// windows.
class ArcWmMetrics::WindowCloseObserver : public aura::WindowObserver {
 public:
  WindowCloseObserver(aura::Window* window, base::OnceClosure callback)
      : window_close_completed_callback_(std::move(callback)) {
    window_observation_.Observe(window);
  }

  WindowCloseObserver(const WindowCloseObserver&) = delete;
  WindowCloseObserver& operator=(const WindowCloseObserver) = delete;
  ~WindowCloseObserver() override = default;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    RecordWindowCloseDelay();
    std::move(window_close_completed_callback_).Run();
  }

 private:
  void RecordWindowCloseDelay() {
    base::UmaHistogramCustomTimes(
        ArcWmMetrics::GetArcWindowClosedTimeHistogramName(),
        window_close_elapsed_timer_.Elapsed(),
        /*minimum=*/base::Milliseconds(1),
        /*maximum=*/base::Seconds(3), 100);
  }

  // Tracks the elapsed time from the window closing operation happens until the
  // the window is destroyed.
  base::ElapsedTimer window_close_elapsed_timer_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::OnceClosure window_close_completed_callback_;
};

// A window observer that records the latency of window bounds change when
// display rotates in tablet mode.
class ArcWmMetrics::WindowRotationObserver : public aura::WindowObserver {
 public:
  WindowRotationObserver(aura::Window* window, base::OnceClosure callback)
      : window_(window),
        window_bounds_changed_completed_callback_(std::move(callback)) {
    window_observation_.Observe(window);
  }

  WindowRotationObserver(const WindowRotationObserver&) = delete;
  WindowRotationObserver& operator=(const WindowRotationObserver) = delete;
  ~WindowRotationObserver() override = default;

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    if (new_bounds ==
        ash::screen_util::GetMaximizedWindowBoundsInParent(window)) {
      RecordWindowRotateDelay();
      std::move(window_bounds_changed_completed_callback_).Run();
    }
  }

 private:
  void RecordWindowRotateDelay() {
    const chromeos::AppType app_type =
        window_->GetProperty(chromeos::kAppTypeKey);
    base::UmaHistogramCustomTimes(
        ArcWmMetrics::GetWindowRotateTimeHistogramName(app_type),
        window_bounds_change_elapsed_timer_.Elapsed(),
        /*minimum=*/base::Milliseconds(1),
        /*maximum=*/base::Seconds(2), 100);
  }

  const raw_ptr<aura::Window> window_;

  // Tracks the elapsed time from the display rotation happens until the window
  // bounds is changed.
  base::ElapsedTimer window_bounds_change_elapsed_timer_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::OnceClosure window_bounds_changed_completed_callback_;
};

ArcWmMetrics::ArcWmMetrics() {
  if (aura::Env::HasInstance()) {
    env_observation_.Observe(aura::Env::GetInstance());
  }

  if (ash::Shell::HasInstance()) {
    shell_observation_.Observe(ash::Shell::Get());
  }
}

ArcWmMetrics::~ArcWmMetrics() = default;

// static
std::string ArcWmMetrics::GetWindowMaximizedTimeHistogramName(
    chromeos::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat({kWindowMaximizedTimeHistogramPrefix, app_type_str});
}

// static
std::string ArcWmMetrics::GetWindowMinimizedTimeHistogramName(
    chromeos::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat({kWindowMinimizedTimeHistogramPrefix, app_type_str});
}

// static
std::string ArcWmMetrics::GetArcWindowClosedTimeHistogramName() {
  const std::string arc_app_type_str =
      GetAppTypeName(chromeos::AppType::ARC_APP);
  return base::StrCat({kWindowClosedTimeHistogramPrefix, arc_app_type_str});
}

// static
std::string ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(
    chromeos::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat(
      {kWindowEnterTabletModeTimeHistogramPrefix, app_type_str});
}

// static
std::string ArcWmMetrics::GetWindowExitTabletModeTimeHistogramName(
    chromeos::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat({kWindowExitTabletModeTimeHistogramPrefix, app_type_str});
}

// static
std::string ArcWmMetrics::GetWindowRotateTimeHistogramName(
    chromeos::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat({kWindowRotateTimeHistogramPrefix, app_type_str});
}

void ArcWmMetrics::OnWindowInitialized(aura::Window* new_window) {
  chromeos::AppType app_type = new_window->GetProperty(chromeos::kAppTypeKey);
  if (app_type == chromeos::AppType::NON_APP) {
    return;
  }

  if (window_observations_.IsObservingSource(new_window)) {
    return;
  }

  window_observations_.AddObservation(new_window);

  if (app_type == chromeos::AppType::ARC_APP) {
    auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(new_window);

    // |shell_surface_base| can be null in unit tests.
    if (shell_surface_base) {
      shell_surface_base->set_pre_close_callback(
          base::BindRepeating(&ArcWmMetrics::OnWindowCloseRequested,
                              weak_ptr_factory_.GetWeakPtr(), new_window));
    }
  }
}

void ArcWmMetrics::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (key != aura::client::kShowStateKey) {
    return;
  }

  if (state_change_observing_windows_.contains(window)) {
    return;
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  const auto new_window_show_state =
      window->GetProperty(aura::client::kShowStateKey);
  const auto old_window_show_state =
      static_cast<ui::mojom::WindowShowState>(old);

  // We do not measure the case that window state is maximized on the app is
  // launched.
  if (new_window_show_state == old_window_show_state) {
    return;
  }

  if (chromeos::ToWindowShowState(
          ash::WindowState::Get(window)->GetStateType()) ==
      new_window_show_state) {
    return;
  }

  const bool from_normal_to_maximized =
      IsNormalWindowStateType(
          chromeos::ToWindowStateType(old_window_show_state)) &&
      new_window_show_state == ui::mojom::WindowShowState::kMaximized;
  const bool from_any_to_minimized =
      new_window_show_state == ui::mojom::WindowShowState::kMinimized;
  if (from_normal_to_maximized || from_any_to_minimized) {
    state_change_observing_windows_.emplace(
        window, std::make_unique<WindowStateChangeObserver>(
                    window, old_window_show_state,
                    base::BindOnce(&ArcWmMetrics::OnOperationCompleted,
                                   base::Unretained(this), window)));
  }

  // The WindowExitTabletModeDelayTime histogram only records data when a
  // maximized window becomes a normal window when exiting tablet mode.
  // Therefore, if a window remains maximized after entering into clamshell
  // mode, we do not need to collect data for that window. This filter is only
  // for client-controlled windows.
  if (exiting_tablet_mode_observing_windows_.contains(window) &&
      ash::WindowState::Get(window)->GetStateType() ==
          chromeos::WindowStateType::kMaximized) {
    exiting_tablet_mode_observing_windows_.erase(window);
  }
}

void ArcWmMetrics::OnWindowDestroying(aura::Window* window) {
  state_change_observing_windows_.erase(window);
  exiting_tablet_mode_observing_windows_.erase(window);
  rotation_observing_windows_.erase(window);
  if (window_observations_.IsObservingSource(window)) {
    window_observations_.RemoveObservation(window);
  }
}

void ArcWmMetrics::OnDisplayTabletStateChanged(display::TabletState state) {
  if (state == display::TabletState::kInClamshellMode) {
    return;
  }

  // After entering tablet mode, we start observing screen rotation.
  if (state == display::TabletState::kInTabletMode) {
    ash::ScreenRotationAnimator* animator =
        ash::RootWindowController::ForWindow(ash::Shell::GetPrimaryRootWindow())
            ->GetScreenRotationAnimator();
    if (animator &&
        !screen_rotation_observations_.IsObservingSource(animator)) {
      screen_rotation_observations_.AddObservation(animator);
    }
    return;
  }

  // When entering or exiting tablet mode, we get the top non floated window and
  // measure the window state change latency for it.
  aura::Window* top_window = ash::window_util::GetTopNonFloatedWindow();
  if (!top_window) {
    return;
  }

  chromeos::WindowStateType window_state_type =
      ash::WindowState::Get(top_window)->GetStateType();

  if (state == display::TabletState::kEnteringTabletMode &&
      IsNormalWindowStateType(window_state_type)) {
    state_change_observing_windows_.emplace(
        top_window,
        std::make_unique<WindowStateChangeObserver>(
            top_window, top_window->GetProperty(aura::client::kShowStateKey),
            base::BindOnce(&ArcWmMetrics::OnOperationCompleted,
                           base::Unretained(this), top_window)));
  } else if (state == display::TabletState::kExitingTabletMode &&
             window_state_type == chromeos::WindowStateType::kMaximized) {
    exiting_tablet_mode_observing_windows_.emplace(
        top_window,
        std::make_unique<WindowStateChangeObserver>(
            top_window, top_window->GetProperty(aura::client::kShowStateKey),
            base::BindOnce(&ArcWmMetrics::OnOperationCompleted,
                           base::Unretained(this), top_window)));
  }
}

void ArcWmMetrics::OnScreenCopiedBeforeRotation() {
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  aura::Window* top_window = ash::window_util::GetTopNonFloatedWindow();
  if (!top_window) {
    return;
  }

  chromeos::WindowStateType window_state_type =
      ash::WindowState::Get(top_window)->GetStateType();
  // We only collect data for the rotation of maximized window.
  if (window_state_type == chromeos::WindowStateType::kMaximized) {
    rotation_observing_windows_.emplace(
        top_window,
        std::make_unique<WindowRotationObserver>(
            top_window, base::BindOnce(&ArcWmMetrics::OnWindowRotationCompleted,
                                       base::Unretained(this), top_window)));
  }
}

void ArcWmMetrics::OnScreenRotationAnimationFinished(
    ash::ScreenRotationAnimator* animator,
    bool canceled) {
  rotation_observing_windows_.clear();
}

void ArcWmMetrics::OnRootWindowWillShutdown(aura::Window* root_window) {
  if (auto* const animator = ash::RootWindowController::ForWindow(root_window)
                                 ->GetScreenRotationAnimator();
      animator && screen_rotation_observations_.IsObservingSource(animator)) {
    screen_rotation_observations_.RemoveObservation(animator);
  }
}

void ArcWmMetrics::OnShellDestroying() {
  shell_observation_.Reset();
  screen_rotation_observations_.RemoveAllObservations();
}

void ArcWmMetrics::OnOperationCompleted(aura::Window* window) {
  state_change_observing_windows_.erase(window);
  exiting_tablet_mode_observing_windows_.erase(window);
}

void ArcWmMetrics::OnWindowRotationCompleted(aura::Window* window) {
  rotation_observing_windows_.erase(window);
}

void ArcWmMetrics::OnWindowCloseRequested(aura::Window* window) {
  close_observing_windows_.emplace(
      window, std::make_unique<WindowCloseObserver>(
                  window, base::BindOnce(&ArcWmMetrics::OnWindowCloseCompleted,
                                         base::Unretained(this), window)));
}

void ArcWmMetrics::OnWindowCloseCompleted(aura::Window* window) {
  close_observing_windows_.erase(window);
}

}  // namespace arc
