// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_WM_METRICS_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_WM_METRICS_H_

#include <memory>
#include <string>

#include "ash/rotator/screen_rotation_animator.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/base/app_types.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace arc {

class ArcWmMetrics : public aura::EnvObserver,
                     public aura::WindowObserver,
                     public ash::ScreenRotationAnimatorObserver,
                     public ash::ShellObserver,
                     public display::DisplayObserver {
 public:
  ArcWmMetrics();
  ArcWmMetrics(const ArcWmMetrics&) = delete;
  ArcWmMetrics& operator=(const ArcWmMetrics&) = delete;
  ~ArcWmMetrics() override;

  static std::string GetWindowMaximizedTimeHistogramName(
      chromeos::AppType app_type);

  static std::string GetWindowMinimizedTimeHistogramName(
      chromeos::AppType app_type);

  static std::string GetArcWindowClosedTimeHistogramName();

  static std::string GetWindowEnterTabletModeTimeHistogramName(
      chromeos::AppType app_type);

  static std::string GetWindowExitTabletModeTimeHistogramName(
      chromeos::AppType app_type);

  static std::string GetWindowRotateTimeHistogramName(
      chromeos::AppType app_type);

  // aura::EnvObserver
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // ash::ScreenRotationAnimatorObserver:
  void OnScreenCopiedBeforeRotation() override;
  void OnScreenRotationAnimationFinished(ash::ScreenRotationAnimator* animator,
                                         bool canceled) override;

  // ash::ShellObserver:
  void OnRootWindowWillShutdown(aura::Window* root_window) override;
  void OnShellDestroying() override;

 private:
  friend class ArcWmMetricsTest;

  class WindowStateChangeObserver;

  class WindowCloseObserver;

  class WindowRotationObserver;

  void OnOperationCompleted(aura::Window* window);
  void OnWindowRotationCompleted(aura::Window* window);

  void OnWindowCloseRequested(aura::Window* window);
  void OnWindowCloseCompleted(aura::Window* window);

  // The map of windows that being observed by WindowStateChangeObserver and
  // their corresponding observers.
  base::flat_map<aura::Window*, std::unique_ptr<WindowStateChangeObserver>>
      state_change_observing_windows_;

  // The map of windows that being observed by WindowCloseObserver and
  // their corresponding observers.
  base::flat_map<aura::Window*, std::unique_ptr<WindowCloseObserver>>
      close_observing_windows_;

  // The map of windows that are exiting tablet mode and being observed by
  // WindowStateChangeObserver, and their corresponding observers.
  base::flat_map<aura::Window*, std::unique_ptr<WindowStateChangeObserver>>
      exiting_tablet_mode_observing_windows_;

  // The map of windows that being observed by WindowRotationObserver and
  // their corresponding observers.
  base::flat_map<aura::Window*, std::unique_ptr<WindowRotationObserver>>
      rotation_observing_windows_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  display::ScopedDisplayObserver display_observer_{this};

  base::ScopedMultiSourceObservation<ash::ScreenRotationAnimator,
                                     ash::ScreenRotationAnimatorObserver>
      screen_rotation_observations_{this};

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};

  base::WeakPtrFactory<ArcWmMetrics> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_WM_METRICS_H_
