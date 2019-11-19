// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_boot_phase_throttle_observer.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"

namespace arc {

ArcBootPhaseThrottleObserver::ArcBootPhaseThrottleObserver()
    : ThrottleObserver(ThrottleObserver::PriorityLevel::CRITICAL,
                       "ArcIsBooting") {}

void ArcBootPhaseThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  DCHECK(!boot_phase_monitor_);
  ThrottleObserver::StartObserving(context, callback);

  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  session_manager->AddObserver(this);

  boot_phase_monitor_ =
      ArcBootPhaseMonitorBridge::GetForBrowserContext(context);
  DCHECK(boot_phase_monitor_);
  boot_phase_monitor_->AddObserver(this);

  SessionRestore::AddObserver(this);
}

void ArcBootPhaseThrottleObserver::StopObserving() {
  SessionRestore::RemoveObserver(this);

  boot_phase_monitor_->RemoveObserver(this);
  boot_phase_monitor_ = nullptr;

  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  session_manager->RemoveObserver(this);

  ThrottleObserver::StopObserving();
}

void ArcBootPhaseThrottleObserver::OnArcStarted() {
  arc_is_booting_ = true;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnArcInitialStart() {
  arc_is_booting_ = false;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnArcSessionRestarting() {
  arc_is_booting_ = true;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnBootCompleted() {
  arc_is_booting_ = false;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnSessionRestoreStartedLoadingTabs() {
  session_restore_loading_ = true;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnSessionRestoreFinishedLoadingTabs() {
  session_restore_loading_ = false;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::MaybeSetActive() {
  if (!arc_is_booting_) {
    // Skip other checks if ARC is not currently booting.
    SetActive(false);
    return;
  }
  auto* profile = Profile::FromBrowserContext(context());
  const bool enabled_by_policy =
      IsArcPlayStoreEnabledForProfile(profile) &&
      IsArcPlayStoreEnabledPreferenceManagedForProfile(profile);

  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  const bool opt_in_boot = !session_manager->is_directly_started();

  // ARC should be always be unthrottled during boot if ARC is enabled by
  // managed policy, or if this is the opt-in boot. Else, only unthrottle ARC
  // if a session restore is not currently taking place.
  const bool always_unthrottle = enabled_by_policy || opt_in_boot;
  const bool active = always_unthrottle || !session_restore_loading_;
  SetActive(active);
}

}  // namespace arc
