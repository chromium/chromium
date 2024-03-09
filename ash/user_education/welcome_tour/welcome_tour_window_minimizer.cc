// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_window_minimizer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_state.h"
#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns whether a window's children should be minimized while
// `WelcomeTourWindowMinimizer` exists.
bool ShouldMinimizeChildren(aura::Window* window) {
  return window && (desks_util::IsDeskContainer(window) ||
                    window->GetId() == kShellWindowId_FloatContainer);
}

// Minimizes a window, if it is possible to do so.
void Minimize(aura::Window* window) {
  auto* state = WindowState::Get(window);
  if (state && !state->IsMinimized()) {
    state->Minimize();
  }
}

// Minimizes all windows in a `aura::WindowTracker`, if they are in containers
// that should have their windows minimized. Can be called asynchronously.
void MaybeMinimize(aura::WindowTracker* window_tracker) {
  for (aura::Window* window : window_tracker->windows()) {
    if (ShouldMinimizeChildren(window->parent())) {
      wm::ScopedAnimationDisabler animation_disabler(window);
      Minimize(window);
    }
  }
}

}  // namespace

// WelcomeTourWindowMinimizer --------------------------------------------------

WelcomeTourWindowMinimizer::WelcomeTourWindowMinimizer() {
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    OnRootWindowAdded(root_window);
  }

  shell_observation_.Observe(Shell::Get());
}

WelcomeTourWindowMinimizer::~WelcomeTourWindowMinimizer() = default;

void WelcomeTourWindowMinimizer::OnWindowDestroying(aura::Window* window) {
  root_window_observations_.RemoveObservation(window);
}

void WelcomeTourWindowMinimizer::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // If the relevant window is entering a container that should be minimized,
  // begin keeping it minimized.
  if (ShouldMinimizeChildren(params.new_parent)) {
    app_window_minimizer_.AddWindow(params.target);
  } else if (ShouldMinimizeChildren(params.old_parent)) {
    // If the window is leaving a should-minimize container and not entering
    // another one, stop keeping it minimized.
    app_window_minimizer_.RemoveWindow(params.target);
  }
}

void WelcomeTourWindowMinimizer::OnRootWindowAdded(aura::Window* root_window) {
  root_window_observations_.AddObservation(root_window);

  std::vector<aura::Window*> containers =
      desks_util::GetDesksContainers(root_window);

  auto* float_container =
      root_window->GetChildById(kShellWindowId_FloatContainer);
  CHECK(float_container);
  containers.push_back(float_container);

  for (auto* container : containers) {
    if (ShouldMinimizeChildren(container)) {
      for (aura::Window* child : container->children()) {
        app_window_minimizer_.AddWindow(child);
      }
    }
  }
}

// WelcomeTourMinimized::AppWindowMinimizer ------------------------------------

WelcomeTourWindowMinimizer::AppWindowMinimizer::AppWindowMinimizer() = default;

WelcomeTourWindowMinimizer::AppWindowMinimizer::~AppWindowMinimizer() = default;

void WelcomeTourWindowMinimizer::AppWindowMinimizer::AddWindow(
    aura::Window* window) {
  Minimize(window);
  if (!app_window_observations_.IsObservingSource(window)) {
    app_window_observations_.AddObservation(window);
  }
}

void WelcomeTourWindowMinimizer::AppWindowMinimizer::RemoveWindow(
    aura::Window* window) {
  if (app_window_observations_.IsObservingSource(window)) {
    app_window_observations_.RemoveObservation(window);
  }
}

void WelcomeTourWindowMinimizer::AppWindowMinimizer::OnWindowDestroying(
    aura::Window* window) {
  app_window_observations_.RemoveObservation(window);
}

void WelcomeTourWindowMinimizer::AppWindowMinimizer::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (visible) {
    // Minimize the window asynchronously to avoid changing visibility directly
    // from within a visibility event.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MaybeMinimize,
                       base::Owned(std::make_unique<aura::WindowTracker>(
                           aura::WindowTracker::WindowList{window}))));
  }
}

}  // namespace ash
