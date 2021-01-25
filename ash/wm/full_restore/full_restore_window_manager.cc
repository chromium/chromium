// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_window_manager.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/full_restore/full_restore_controller.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

namespace {

// The list of possible app window parents.
// TODO(crbug.com/1164472): Support the rest of the desk containers which
// are currently not always created depending on whether the bento feature
// is enabled.
constexpr ShellWindowId kAppParentContainers[5] = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
    kShellWindowId_AlwaysOnTopContainer,
};

// The types of apps currently supported by full restore.
// TODO(crbug.com/1164472): Add ARC apps when they are supported.
// TODO(crbug.com/1164472): Checking app type is temporary solution until we
// can get windows which are allowed to full restore from the
// FullRestoreService.
constexpr AppType kSupportedAppTypes[2] = {AppType::BROWSER,
                                           AppType::CHROME_APP};

}  // namespace

FullRestoreWindowManager::FullRestoreWindowManager() {
  // TODO(crbug.com/1164472): SetEnabled should be called from
  // FullRestoreController. For now this is ok as the feature is disabled by
  // default.
  SetEnabled(true);
}

FullRestoreWindowManager::~FullRestoreWindowManager() {
  StopObserving();
}

void FullRestoreWindowManager::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;

  if (!enabled) {
    // TODO(crbug.com/1164472): Clear database.
    StopObserving();
    return;
  }

  DCHECK(!app_window_parents_observations_.IsObservingAnySource());
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    for (ShellWindowId id : kAppParentContainers) {
      aura::Window* child = root_window->GetChildById(id);
      DCHECK(child);
      app_window_parents_observations_.AddObservation(child);
    }
  }

  // TODO(crbug.com/1164472): Start observing and save existing app windows in
  // the MRU list to the database.
}

void FullRestoreWindowManager::OnWindowAdded(aura::Window* new_window) {
  DCHECK(app_window_parents_observations_.IsObservingAnySource());
  DCHECK(new_window->parent());

  if (!app_window_parents_observations_.IsObservingSource(new_window->parent()))
    return;

  // TODO(crbug.com/1164472): For browser and chrome apps, the window property
  // is set after the window is created. Change those apps to set the property
  // using Widget's |init_properties_container| so the property can be extracted
  // at this time.
  if (!base::Contains(kSupportedAppTypes,
                      static_cast<AppType>(
                          new_window->GetProperty(aura::client::kAppType)))) {
    return;
  }

  // The window is already observed if it is switching containers (i.e. moving
  // window between desks).
  if (!app_window_observations_.IsObservingSource(new_window)) {
    app_window_observations_.AddObservation(new_window);

    auto* new_window_state = WindowState::Get(new_window);
    DCHECK(!app_window_state_observations_.IsObservingSource(new_window_state));
    app_window_state_observations_.AddObservation(new_window_state);
  }

  FullRestoreController::Get()->SaveWindows();
}

void FullRestoreWindowManager::OnWindowDestroying(aura::Window* window) {
  // Do nothing if windows in |app_window_parents_| are destroyed. |this| will
  // be destroyed shortly and their observers will be cleaned up then.
  if (app_window_parents_observations_.IsObservingSource(window)) {
    app_window_parents_observations_.RemoveObservation(window);
    return;
  }

  auto* window_state = WindowState::Get(window);
  DCHECK(app_window_observations_.IsObservingSource(window));
  DCHECK(app_window_state_observations_.IsObservingSource(window_state));

  app_window_observations_.RemoveObservation(window);
  app_window_state_observations_.RemoveObservation(window_state);

  FullRestoreController::Get()->SaveWindows();
}

void FullRestoreWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!app_window_observations_.IsObservingSource(window))
    return;

  if (reason == ui::PropertyChangeReason::FROM_ANIMATION)
    return;

  // TODO(crbug.com/1164472): Investigate if we can skip bounds change updates
  // that happen during a window drag.

  FullRestoreController::Get()->SaveWindows();
}

void FullRestoreWindowManager::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  if (!app_window_observations_.IsObservingSource(window))
    return;

  FullRestoreController::Get()->SaveWindows();
}

void FullRestoreWindowManager::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  DCHECK(app_window_state_observations_.IsObservingSource(window_state));

  // TODO(crbug.com/1164472): We may not be interested in all window state
  // changes.

  FullRestoreController::Get()->SaveWindows();
}

void FullRestoreWindowManager::StopObserving() {
  app_window_parents_observations_.RemoveAllObservations();
  app_window_observations_.RemoveAllObservations();
  app_window_state_observations_.RemoveAllObservations();
}

}  // namespace ash
