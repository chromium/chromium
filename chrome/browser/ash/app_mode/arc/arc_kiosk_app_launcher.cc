// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_launcher.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/env.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"

namespace ash {

ArcKioskAppLauncher::ArcKioskAppLauncher(content::BrowserContext* context,
                                         ArcAppListPrefs* prefs,
                                         const std::string& app_id,
                                         Delegate* delegate)
    : app_id_(app_id), prefs_(prefs), delegate_(delegate) {
  prefs_->AddObserver(this);
  exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
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
                                        const std::string& intent,
                                        int32_t session_id) {
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

void ArcKioskAppLauncher::OnExoWindowCreated(aura::Window* window) {
  window->AddObserver(this);
  windows_.insert(window);

  // The |window|’s task ID might not be set yet. Record the window and
  // OnTaskCreated will check if it's the window we're looking for.
  if (task_id_ == -1)
    return;

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
  PinWindow(window, /*trusted=*/true);
  if (delegate_)
    delegate_->OnAppWindowLaunched();
  return true;
}

void ArcKioskAppLauncher::StopObserving() {
  exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
  for (auto* window : windows_)
    window->RemoveObserver(this);
  windows_.clear();
  prefs_->RemoveObserver(this);
}

}  // namespace ash
