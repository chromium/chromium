// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"

#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"

namespace arc {

ArcSystemStateObservation::ArcSystemStateObservation(
    content::BrowserContext* context)
    : ThrottleService(context) {
  // TODO(sstan): Use ARC window observer after it's landed.
  AddObserver(std::make_unique<ArcActiveWindowThrottleObserver>());

  // Observe background service in ARC side.
  AddObserver(std::make_unique<ArcBackgroundServiceObserver>());

  // Observe ARC window in ash.
  AddObserver(std::make_unique<ArcWindowObserver>());
}

ArcSystemStateObservation::~ArcSystemStateObservation() = default;

void ArcSystemStateObservation::ThrottleInstance(bool should_throttle) {
  // ARC system or app is active.
  if (!should_throttle) {
    last_peace_timestamp_.reset();
    if (!active_callback_.is_null()) {
      active_callback_.Run();
    }
    return;
  }

  // ARC system and app is not active.
  last_peace_timestamp_ = base::Time::Now();
}

absl::optional<base::TimeDelta> ArcSystemStateObservation::GetPeaceDuration() {
  if (!last_peace_timestamp_.has_value()) {
    return absl::nullopt;
  }
  return base::Time::Now() - *last_peace_timestamp_;
}

void ArcSystemStateObservation::SetDurationResetCallback(
    base::RepeatingClosure cb) {
  active_callback_ = std::move(cb);
}

base::WeakPtr<ArcSystemStateObservation>
ArcSystemStateObservation::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace arc
