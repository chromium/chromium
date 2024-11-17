// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"

namespace arc {

ArcSystemStateObservation::ArcSystemStateObservation(
    content::BrowserContext* context)
    : ThrottleService(context) {
  // Observe ARC running state. PlayStore ready roughly means the ARC app is
  // ready to be launched by the user.
  app_prefs_observation_.Observe(ArcAppListPrefs::Get(context));

  // TODO(sstan): Use ARC window observer after it's landed.
  AddObserver(std::make_unique<ArcActiveWindowThrottleObserver>());

  // Observe background service in ARC side.
  AddObserver(std::make_unique<ArcBackgroundServiceObserver>());

  // Observe ARC window in ash.
  AddObserver(std::make_unique<ArcWindowObserver>());

  StartObservers();
}

ArcSystemStateObservation::~ArcSystemStateObservation() = default;

void ArcSystemStateObservation::ThrottleInstance(bool should_throttle) {
  // ARC system or app is active.
  if (!should_throttle) {
    last_peace_timestamp_.reset();
    DVLOG(1) << "ARC is active, reset peace timestamp.";
    if (!active_callback_.is_null()) {
      active_callback_.Run();
    }
    return;
  }

  // Only update when ARC is running. If ARC haven't booted, the "inactive"
  // state does not make any sense.
  if (arc_running_) {
    // ARC system and app is not active.
    last_peace_timestamp_ = base::Time::Now();
    DVLOG(1) << "ARC is not active, time recording start at "
             << last_peace_timestamp_.value();
  }
}

void ArcSystemStateObservation::OnAppStatesChanged(
    const std::string& id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (id != arc::kPlayStoreAppId || app_info.ready == arc_running_) {
    return;
  }
  if (app_info.ready) {
    arc_running_ = true;
    if (should_throttle()) {
      last_peace_timestamp_ = base::Time::Now();
    }
  } else {
    arc_running_ = false;
    last_peace_timestamp_.reset();
  }
}

void ArcSystemStateObservation::OnArcAppListPrefsDestroyed() {
  app_prefs_observation_.Reset();
}

std::optional<base::TimeDelta> ArcSystemStateObservation::GetPeaceDuration() {
  if (!last_peace_timestamp_.has_value()) {
    return std::nullopt;
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
