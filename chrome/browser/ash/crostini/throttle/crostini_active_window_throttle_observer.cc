// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/throttle/crostini_active_window_throttle_observer.h"

#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace crostini {

CrostiniActiveWindowThrottleObserver::CrostiniActiveWindowThrottleObserver()
    : WindowThrottleObserverBase("CrostiniWindowIsActiveWindow") {}

bool CrostiniActiveWindowThrottleObserver::ProcessWindowActivation(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active) {
    return false;
  }

  // Return true if the gained_active window is a Crostini app.
  if (gained_active->GetProperty(chromeos::kAppTypeKey) ==
      chromeos::AppType::CROSTINI_APP) {
    return true;
  }

  // Return false if the window is not a Chrome app (e.g. the browser, ARC app.)
  if (gained_active->GetProperty(chromeos::kAppTypeKey) !=
      chromeos::AppType::CHROME_APP) {
    return false;
  }

  // Return true if the ID is the terminal app's. Note that the terminal app is
  // a Chrome app although it provides a Crostini shell.
  const std::string* app_id = gained_active->GetProperty(ash::kAppIDKey);
  return app_id && *app_id == guest_os::kTerminalSystemAppId;
}

}  // namespace crostini
