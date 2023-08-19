// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_WINDOW_MINIMIZER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_WINDOW_MINIMIZER_H_

#include "ash/ash_export.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"

namespace ash {

// The class, owned by `WelcomeTourController`, that is responsible for keeping
// windows minimized during the tour to prevent noise. While it exists, any
// window that is a child of the desk or float containers that attempts to show
// will be immediately minimized. On destruction, windows will stay minimized,
// but won't be stopped from being unminimized.
class ASH_EXPORT WelcomeTourWindowMinimizer : public aura::WindowObserver,
                                              public ShellObserver {
 public:
  WelcomeTourWindowMinimizer();
  WelcomeTourWindowMinimizer(const WelcomeTourWindowMinimizer&) = delete;
  WelcomeTourWindowMinimizer& operator=(const WelcomeTourWindowMinimizer&) =
      delete;
  ~WelcomeTourWindowMinimizer() override;

 private:
  // The class that minimizes app windows. This behavior is in its own class to
  // separate app window observation behavior from that of root windows.
  class AppWindowMinimizer : public aura::WindowObserver {
   public:
    AppWindowMinimizer();
    AppWindowMinimizer(const AppWindowMinimizer&) = delete;
    AppWindowMinimizer& operator=(const AppWindowMinimizer&) = delete;
    ~AppWindowMinimizer() override;

    void AddWindow(aura::Window* window);
    void RemoveWindow(aura::Window* window);

   private:
    // aura::WindowObserver:
    void OnWindowDestroying(aura::Window* window) override;
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

    base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
        app_window_observations_{this};
  };

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;

  AppWindowMinimizer app_window_minimizer_;

  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      root_window_observations_{this};
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_WINDOW_MINIMIZER_H_
