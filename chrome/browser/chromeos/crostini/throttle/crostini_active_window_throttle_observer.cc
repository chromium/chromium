// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/throttle/crostini_active_window_throttle_observer.h"

#include "ash/public/cpp/app_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace crostini {

CrostiniActiveWindowThrottleObserver::CrostiniActiveWindowThrottleObserver()
    : WindowThrottleObserverBase(ThrottleObserver::PriorityLevel::CRITICAL,
                                 "CrostiniWindowIsActiveWindow") {}

bool CrostiniActiveWindowThrottleObserver::ProcessWindowActivation(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active)
    return false;
  // Return true if the gained_active window is a Crostini app.
  return gained_active->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::CROSTINI_APP);
}

}  // namespace crostini
