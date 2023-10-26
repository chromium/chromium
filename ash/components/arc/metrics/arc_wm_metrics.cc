// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_wm_metrics.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"

namespace arc {

namespace {

// Histogram of the delay for window maximizing operation.
constexpr char kWindowMaximizedTimeHistogramPrefix[] =
    "Arc.WM.WindowMaximizedDelayTime.";
// Histogram of the delay for window minimizing operation.
constexpr char kWindowMinimizedTimeHistogramPrefix[] =
    "Arc.WM.WindowMinimizedDelayTime.";
// Histogram of the delay for window closing operation.
constexpr char kWindowClosedTimeHistogramPrefix[] =
    "Arc.WM.WindowClosedDelayTime.";
// Histogram of the delay for window state transition when entering into tablet
// mode.
constexpr char kWindowEnterTabletModeTimeHistogramPrefix[] =
    "Arc.WM.WindowEnterTabletModeDelayTime.";

constexpr char kArcHistogramName[] = "ArcApp";
constexpr char kBrowserHistogramName[] = "Browser";
constexpr char kChromeAppHistogramName[] = "ChromeApp";
constexpr char kSystemAppHistogramName[] = "SystemApp";
constexpr char kCrostiniAppHistogramName[] = "CrostiniApp";

std::string GetAppTypeName(ash::AppType app_type) {
  switch (app_type) {
    case ash::AppType::ARC_APP:
      return kArcHistogramName;
    case ash::AppType::BROWSER:
      return kBrowserHistogramName;
    case ash::AppType::CHROME_APP:
      return kChromeAppHistogramName;
    case ash::AppType::SYSTEM_APP:
      return kSystemAppHistogramName;
    case ash::AppType::CROSTINI_APP:
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
                            ui::WindowShowState old_window_show_state,
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
    if (old_window_state_type ==
        chromeos::ToWindowStateType(old_window_show_state_)) {
      RecordWindowStateChangeDelay(new_window_state);
    }

    std::move(window_operation_completed_callback_).Run();
  }

 private:
  void RecordWindowStateChangeDelay(ash::WindowState* state) {
    const ash::AppType app_type =
        static_cast<ash::AppType>(window_->GetProperty(aura::client::kAppType));
    if (ash::Shell::Get()->tablet_mode_controller()->InTabletMode()) {
      // When entering tablet mode, we only collect the data of visible window.
      if (state->IsMaximized() && window_->IsVisible()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(2), 100);
      }
    } else {
      if (state->IsMaximized()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowMaximizedTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(2), 100);
      } else if (state->IsMinimized()) {
        base::UmaHistogramCustomTimes(
            ArcWmMetrics::GetWindowMinimizedTimeHistogramName(app_type),
            window_operation_elapsed_timer_.Elapsed(),
            /*minimum=*/base::Milliseconds(1),
            /*maximum=*/base::Seconds(2), 100);
      }
    }
  }

  const raw_ptr<aura::Window, ExperimentalAsh> window_;
  const ui::WindowShowState old_window_show_state_;

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
        /*maximum=*/base::Seconds(2), 100);
  }

  // Tracks the elapsed time from the window closing operation happens until the
  // the window is destroyed.
  base::ElapsedTimer window_close_elapsed_timer_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::OnceClosure window_close_completed_callback_;
};

ArcWmMetrics::ArcWmMetrics() {
  if (aura::Env::HasInstance()) {
    env_observation_.Observe(aura::Env::GetInstance());
  }

  if (ash::Shell::HasInstance()) {
    tablet_mode_observation_.Observe(
        ash::Shell::Get()->tablet_mode_controller());
  }
}

ArcWmMetrics::~ArcWmMetrics() = default;

// static
std::string ArcWmMetrics::GetWindowMaximizedTimeHistogramName(
    ash::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat({kWindowMaximizedTimeHistogramPrefix, app_type_str});
}

// static
std::string ArcWmMetrics::GetWindowMinimizedTimeHistogramName(
    ash::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat({kWindowMinimizedTimeHistogramPrefix, app_type_str});
}

// static
std::string ArcWmMetrics::GetArcWindowClosedTimeHistogramName() {
  const std::string arc_app_type_str = GetAppTypeName(ash::AppType::ARC_APP);
  return base::StrCat({kWindowClosedTimeHistogramPrefix, arc_app_type_str});
}

// static
std::string ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(
    ash::AppType app_type) {
  const std::string app_type_str = GetAppTypeName(app_type);
  return base::StrCat(
      {kWindowEnterTabletModeTimeHistogramPrefix, app_type_str});
}

void ArcWmMetrics::OnWindowInitialized(aura::Window* new_window) {
  ash::AppType app_type = static_cast<ash::AppType>(
      new_window->GetProperty(aura::client::kAppType));
  if (app_type == ash::AppType::NON_APP) {
    return;
  }

  if (window_observations_.IsObservingSource(new_window)) {
    return;
  }

  window_observations_.AddObservation(new_window);

  if (app_type == ash::AppType::ARC_APP) {
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

  if (ash::Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    return;
  }

  const auto new_window_show_state =
      window->GetProperty(aura::client::kShowStateKey);
  const auto old_window_show_state = static_cast<ui::WindowShowState>(old);

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
      new_window_show_state == ui::WindowShowState::SHOW_STATE_MAXIMIZED;
  const bool from_any_to_minimized =
      new_window_show_state == ui::WindowShowState::SHOW_STATE_MINIMIZED;
  if (from_normal_to_maximized || from_any_to_minimized) {
    state_change_observing_windows_.emplace(
        window, std::make_unique<WindowStateChangeObserver>(
                    window, old_window_show_state,
                    base::BindOnce(&ArcWmMetrics::OnOperationCompleted,
                                   base::Unretained(this), window)));
  }
}

void ArcWmMetrics::OnWindowDestroying(aura::Window* window) {
  state_change_observing_windows_.erase(window);
  if (window_observations_.IsObservingSource(window)) {
    window_observations_.RemoveObservation(window);
  }
}

void ArcWmMetrics::OnTabletModeStarting() {
  aura::Window* top_window = ash::window_util::GetTopNonFloatedWindow();
  if (!top_window) {
    return;
  }

  chromeos::WindowStateType window_state_type =
      ash::WindowState::Get(top_window)->GetStateType();
  if (IsNormalWindowStateType(window_state_type)) {
    state_change_observing_windows_.emplace(
        top_window,
        std::make_unique<WindowStateChangeObserver>(
            top_window, top_window->GetProperty(aura::client::kShowStateKey),
            base::BindOnce(&ArcWmMetrics::OnOperationCompleted,
                           base::Unretained(this), top_window)));
  }
}

void ArcWmMetrics::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void ArcWmMetrics::OnOperationCompleted(aura::Window* window) {
  state_change_observing_windows_.erase(window);
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
