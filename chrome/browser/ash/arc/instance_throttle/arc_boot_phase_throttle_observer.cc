// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"

namespace arc {
namespace {

// Try to throttle ARC |kThrottleArcDelay| after app.mojom is connected.
constexpr base::TimeDelta kThrottleArcDelay = base::Seconds(10);

void RemoveMojoObservers(ArcBootPhaseThrottleObserver* observer) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  arc_service_manager->arc_bridge_service()->intent_helper()->RemoveObserver(
      observer);
  arc_service_manager->arc_bridge_service()->app()->RemoveObserver(observer);
}

}  // namespace

ArcBootPhaseThrottleObserver::ArcBootPhaseThrottleObserver()
    : ThrottleObserver(kArcBootPhaseThrottleObserverName) {}

ArcBootPhaseThrottleObserver::~ArcBootPhaseThrottleObserver() = default;

void ArcBootPhaseThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);

  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  session_manager->AddObserver(this);

  SessionRestore::AddObserver(this);

  // If app() and/or intent_helper() are already connected to the instance in
  // the guest, the OnConnectionReady() function is synchronously called before
  // returning from AddObserver. For more details, see
  // ash/components/arc/session/connection_holder.h especially its AddObserver()
  // function.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  // ArcServiceManager and objects owned by the manager are created very early
  // in `ChromeBrowserMainPartsAsh::PreMainMessageLoopRun()` too.
  DCHECK(arc_service_manager);
  arc_service_manager->arc_bridge_service()->app()->AddObserver(this);
  arc_service_manager->arc_bridge_service()->intent_helper()->AddObserver(this);
}

void ArcBootPhaseThrottleObserver::StopObserving() {
  RemoveMojoObservers(this);
  SessionRestore::RemoveObserver(this);

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

void ArcBootPhaseThrottleObserver::OnSessionRestoreStartedLoadingTabs() {
  session_restore_loading_ = true;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnSessionRestoreFinishedLoadingTabs() {
  session_restore_loading_ = false;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::OnConnectionReady() {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  const bool app_connected =
      arc_service_manager &&
      arc_service_manager->arc_bridge_service()->app()->IsConnected();
  const bool intent_helper_connected =
      arc_service_manager &&
      arc_service_manager->arc_bridge_service()->intent_helper()->IsConnected();
  if (!app_connected || !intent_helper_connected)
    return;

  // Only the first OnConnectionReady() calls need to be monitored. Remove the
  // observers now.
  RemoveMojoObservers(this);

  DVLOG(1)
      << "app.mojom and intent_helper.mojom are connected. Throttle ARC in "
      << kThrottleArcDelay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcBootPhaseThrottleObserver::ThrottleArc,
                     weak_ptr_factory_.GetWeakPtr()),
      kThrottleArcDelay);
}

// static
const base::TimeDelta&
ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting() {
  return kThrottleArcDelay;
}

void ArcBootPhaseThrottleObserver::ThrottleArc() {
  DVLOG(1) << "Throttling ARC (reason: boot completed)";
  arc_is_booting_ = false;
  MaybeSetActive();
}

void ArcBootPhaseThrottleObserver::MaybeSetActive() {
  if (!arc_is_booting_ || !*arc_is_booting_) {
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
  const bool opt_in_boot =
      !session_manager->skipped_terms_of_service_negotiation();

  // ARC should be always be unthrottled during boot if ARC is enabled by
  // managed policy, or if this is the opt-in boot. Else, only unthrottle ARC
  // if a session restore is not currently taking place.
  const bool always_unthrottle = enabled_by_policy || opt_in_boot;
  const bool active = always_unthrottle || !session_restore_loading_;
  SetActive(active);
}

}  // namespace arc
