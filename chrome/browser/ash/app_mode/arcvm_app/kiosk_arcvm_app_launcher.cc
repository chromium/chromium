// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/418858226): Add tests for this file.
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_launcher.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/wm/window_pin_util.h"
#include "base/check.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/env.h"

namespace ash {

KioskArcvmAppLauncher::KioskArcvmAppLauncher(ArcAppListPrefs* prefs,
                                             const std::string& app_id,
                                             Owner* owner)
    : app_id_(app_id), prefs_(prefs), owner_(owner) {
  // TODO(crbug.com/418636317): Refactor to use ScopedObservation. Move the
  // observation logic outside the constructor. Avoid ExoWindow observation.
  prefs_->AddObserver(this);
  exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
}

KioskArcvmAppLauncher::~KioskArcvmAppLauncher() {
  StopObserving();
}

void KioskArcvmAppLauncher::LaunchApp(content::BrowserContext* context) {
  // Launching the app by app id in landscape mode and in non-touch mode.
  arc::LaunchApp(context, app_id_, ui::EF_NONE,
                 arc::UserInteractionType::NOT_USER_INITIATED);
}

void KioskArcvmAppLauncher::OnTaskCreated(int32_t task_id,
                                          const std::string& package_name,
                                          const std::string& activity,
                                          const std::string& intent,
                                          int32_t session_id) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app = prefs_->GetApp(app_id_);
  if (!app || app->package_name != package_name || app->activity != activity) {
    return;
  }
  task_id_ = std::make_optional(task_id);
  // The app window may have been created already.
  for (aura::Window* window : windows_) {
    if (CheckAndPinWindow(window)) {
      break;
    }
  }
}

void KioskArcvmAppLauncher::OnExoWindowCreated(aura::Window* window) {
  window->AddObserver(this);
  windows_.insert(window);

  // The `window`’s task ID might not be set yet. Record the window and
  // OnTaskCreated will check if it's the window we're looking for.
  if (!task_id_.has_value()) {
    return;
  }

  CheckAndPinWindow(window);
}

void KioskArcvmAppLauncher::OnWindowDestroying(aura::Window* window) {
  // TODO(crbug.com/418637197): Refactor to use
  // base/scoped_multi_source_observation.h
  window->RemoveObserver(this);
  windows_.erase(window);
}

// If the correct window is found and pinned, this method makes a call to the
// owner -> OnAppWindowLaunched which, timing wise, might be later than when the
// window is actually created and `OnTaskCreated` is invoked.
bool KioskArcvmAppLauncher::CheckAndPinWindow(aura::Window* const window) {
  CHECK(task_id_.has_value());
  if (task_id_ != arc::GetWindowTaskId(window)) {
    return false;
  }
  // Stop observing as target window is already found.
  StopObserving();
  PinWindow(window, /*trusted=*/true);
  if (owner_) {
    owner_->OnAppWindowLaunched();
  }
  return true;
}

void KioskArcvmAppLauncher::StopObserving() {
  // TODO(crbug.com/418858871): Remove this check.
  if (exo::WMHelper::HasInstance()) {
    // The `HasInstance` check is here, because of a crash where the `WMHelper`
    // was already destroyed. We do not know how we would get into this
    // position. See crbug.com/281992317 for reference.
    exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
  }
  for (aura::Window* window : windows_) {
    window->RemoveObserver(this);
  }
  windows_.clear();
  prefs_->RemoveObserver(this);
}

}  // namespace ash
