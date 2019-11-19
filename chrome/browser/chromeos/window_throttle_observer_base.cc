// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/window_throttle_observer_base.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_change_observer.h"

namespace chromeos {
namespace {

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  if (!window)
    return false;

  const aura::Window* parent = window->parent();
  return parent &&
         parent->id() == ash::ShellWindowId::kShellWindowId_AppListContainer;
}

// Returns true if this window activation should be ignored (app list
// opening/closing), or false if it should be processed.
bool ShouldIgnoreWindowActivation(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  // The current active window defines whether the instance should be
  // throttled or not i.e. if it's a native window then throttle Android, if
  // it's an Android window then unthrottle it.
  //
  // However, the app list needs to be handled differently. The app list is
  // deemed as an temporary overlay over the user's main window for browsing
  // through or selecting an app. Consequently, if the app list is brought
  // over a Chrome window then IsAppListWindow(gained_active) is true and this
  // event is ignored. The instance continues to be throttled. Once the app
  // list is closed then IsAppListWindow(lost_active) is true and the instance
  // continues to be throttled. Similarly, if the app list is brought over an
  // Android window, the instance continues to be unthrottled.
  //
  // On launching an app from the app list on Chrome OS the following events
  // happen -
  //
  // - When an Android app icon is clicked the instance is unthrottled. This
  // logic resides in |LaunchAppWithIntent| in
  // src/chrome/browser/ui/app_list/arc/arc_app_utils.cc.
  //
  // - Between the time the app opens there is a narrow slice of time where
  // this callback is triggered with |lost_active| equal to the app list
  // window and the gained active possibly a native window. Without the check
  // below the instance will be throttled again, further delaying the app
  // launch. If the app was a native one then the instance was throttled
  // anyway.
  if (IsAppListWindow(lost_active) || IsAppListWindow(gained_active))
    return true;
  return false;
}

}  // namespace

WindowThrottleObserverBase::WindowThrottleObserverBase(
    ThrottleObserver::PriorityLevel level,
    std::string name)
    : ThrottleObserver(level, name) {}

void WindowThrottleObserverBase::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  if (!exo::WMHelper::HasInstance())  // for unit testing
    return;
  exo::WMHelper::GetInstance()->AddActivationObserver(this);
}

void WindowThrottleObserverBase::StopObserving() {
  ThrottleObserver::StopObserving();
  if (!exo::WMHelper::HasInstance())
    return;
  exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
}

void WindowThrottleObserverBase::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  if (ShouldIgnoreWindowActivation(reason, gained_active, lost_active))
    return;
  SetActive(ProcessWindowActivation(reason, gained_active, lost_active));
}

}  // namespace chromeos
