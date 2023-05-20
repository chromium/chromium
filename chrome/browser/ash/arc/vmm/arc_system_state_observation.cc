// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"

namespace arc {

ArcSystemStateObservation::ArcSystemStateObservation(
    content::BrowserContext* context)
    : ThrottleService(context) {
  // If app() are already connected to the instance in
  // the guest, the OnConnectionReady() function is synchronously called before
  // returning from AddObserver. For more details, see
  // ash/components/arc/session/connection_holder.h, especially its
  // AddObserver() function.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  // ArcServiceManager and objects owned by the manager are created very early
  // in `ChromeBrowserMainPartsAsh::PreMainMessageLoopRun()` too.
  DCHECK(arc_service_manager);
  arc_service_manager->arc_bridge_service()->app()->AddObserver(this);

  // TODO(sstan): Use ARC window observer after it's landed.
  AddObserver(std::make_unique<ArcActiveWindowThrottleObserver>());

  // Observe background service in ARC side.
  AddObserver(std::make_unique<ArcBackgroundServiceObserver>());

  // Observe ARC window in ash.
  AddObserver(std::make_unique<ArcWindowObserver>());

  StartObservers();
}

ArcSystemStateObservation::~ArcSystemStateObservation() {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  DCHECK(arc_service_manager->arc_bridge_service());
  DCHECK(arc_service_manager->arc_bridge_service()->app());
  arc_service_manager->arc_bridge_service()->app()->RemoveObserver(this);
}

void ArcSystemStateObservation::ThrottleInstance(bool should_throttle) {
  // ARC system or app is active.
  if (!should_throttle) {
    last_peace_timestamp_.reset();
    if (!active_callback_.is_null()) {
      active_callback_.Run();
    }
    return;
  }

  // Only update when ARC is running. If ARC haven't booted, the "inactive"
  // state does not make any sense.
  if (arc_connected_) {
    // ARC system and app is not active.
    last_peace_timestamp_ = base::Time::Now();
  }
}

void ArcSystemStateObservation::OnConnectionReady() {
  arc_connected_ = true;
  if (should_throttle()) {
    last_peace_timestamp_ = base::Time::Now();
  }
}
void ArcSystemStateObservation::OnConnectionClosed() {
  arc_connected_ = false;
  last_peace_timestamp_.reset();
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
