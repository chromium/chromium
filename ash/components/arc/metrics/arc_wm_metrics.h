// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_WM_METRICS_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_WM_METRICS_H_

#include <memory>
#include <string>

#include "ash/constants/app_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace arc {

class ArcWmMetrics : public aura::EnvObserver, public aura::WindowObserver {
 public:
  ArcWmMetrics();
  ArcWmMetrics(const ArcWmMetrics&) = delete;
  ArcWmMetrics& operator=(const ArcWmMetrics&) = delete;
  ~ArcWmMetrics() override;

  static std::string GetWindowMaximizedTimeHistogramName(ash::AppType app_type);

  static std::string GetWindowMinimizedTimeHistogramName(ash::AppType app_type);

  // aura::EnvObserver
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  class WindowStateChangeObserver;

  void OnOperationCompleted(aura::Window* window);

  // The map of windows that being observed by WindowStateChangeObserver and
  // their corresponding observers.
  base::flat_map<aura::Window*, std::unique_ptr<WindowStateChangeObserver>>
      state_change_observing_windows_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_WM_METRICS_H_
