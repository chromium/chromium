// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_display_power_observer.h"

#include "ash/shell.h"

namespace arc {

ArcDisplayPowerObserver::ArcDisplayPowerObserver()
    : ThrottleObserver(kArcDisplayPowerObserverName) {}

void ArcDisplayPowerObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);

  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->display_configurator()->AddObserver(this);
}

void ArcDisplayPowerObserver::StopObserving() {
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->display_configurator()->RemoveObserver(this);

  ThrottleObserver::StopObserving();
}

void ArcDisplayPowerObserver::OnPowerStateChanged(
    chromeos::DisplayPowerState power_state) {
  const bool display_enabled = (power_state != chromeos::DISPLAY_POWER_ALL_OFF);
  SetEnforced(!display_enabled);
}

}  // namespace arc
