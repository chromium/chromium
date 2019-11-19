// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_launcher.h>

#include <memory>
#include <string>

#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "ui/aura/env.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"

namespace chromeos {

ArcKioskAppLauncher::ArcKioskAppLauncher(content::BrowserContext* context,
                                         ArcAppListPrefs* prefs,
                                         const std::string& app_id,
                                         Delegate* delegate)
    : app_id_(app_id), prefs_(prefs), delegate_(delegate) {
  prefs_->AddObserver(this);
  aura::Env::GetInstance()->AddObserver(this);
  // Launching the app by app id in landscape mode and in non-touch mode.
  arc::LaunchApp(context, app_id_, ui::EF_NONE,
                 arc::UserInteractionType::NOT_USER_INITIATED);
}

ArcKioskAppLauncher::~ArcKioskAppLauncher() {
  StopObserving();
}

void ArcKioskAppLauncher::OnTaskCreated(int32_t task_id,
                                        const std::string& package_name,
                                        const std::string& activity,
                                        const std::string& intent) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app = prefs_->GetApp(app_id_);
  if (!app || app->package_name != package_name || app->activity != activity)
    return;
  task_id_ = task_id;
  // The app window may have been created already.
  for (aura::Window* window : windows_) {
    if (CheckAndPinWindow(window))
      break;
  }
}

void ArcKioskAppLauncher::OnWindowInitialized(aura::Window* window) {
  // The |window|’s task ID is not set yet. We need to observe
  // the window until the |kApplicationIdKey| property is set.
  window->AddObserver(this);
  windows_.insert(window);
}

void ArcKioskAppLauncher::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old) {
  // If we do not know yet what task ID to look for, do nothing.
  // Existing windows will be revisited the moment the task ID
  // becomes known.
  if (task_id_ == -1)
    return;

  // We are only interested in changes to |kApplicationIdKey|,
  // but that constant is not accessible outside shell_surface.cc.
  // So we react to all property changes.
  CheckAndPinWindow(window);
}

void ArcKioskAppLauncher::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  windows_.erase(window);
}

bool ArcKioskAppLauncher::CheckAndPinWindow(aura::Window* const window) {
  DCHECK_GE(task_id_, 0);
  if (arc::GetWindowTaskId(window) != task_id_)
    return false;
  // Stop observing as target window is already found.
  StopObserving();
  window->SetProperty(ash::kWindowPinTypeKey,
                      ash::WindowPinType::kTrustedPinned);
  if (delegate_)
    delegate_->OnAppWindowLaunched();
  return true;
}

void ArcKioskAppLauncher::StopObserving() {
  aura::Env::GetInstance()->RemoveObserver(this);
  for (auto* window : windows_)
    window->RemoveObserver(this);
  windows_.clear();
  prefs_->RemoveObserver(this);
}

}  // namespace chromeos
