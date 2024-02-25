// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {
namespace {

void OnEmitArcBooted(bool success) {
  if (!success)
    VLOG(1) << "Failed to emit arc booted signal.";
}

class DefaultDelegateImpl : public ArcBootPhaseMonitorBridge::Delegate {
 public:
  DefaultDelegateImpl() = default;
  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;
  ~DefaultDelegateImpl() override = default;

  void RecordFirstAppLaunchDelayUMA(base::TimeDelta delta) override {
    VLOG(2) << "Launching the first app took "
            << delta.InMillisecondsRoundedUp() << " ms.";
    UMA_HISTOGRAM_CUSTOM_TIMES("Arc.FirstAppLaunchDelay.TimeDelta", delta,
                               base::Milliseconds(1), base::Minutes(2), 50);
  }

  void RecordAppRequestedInSessionUMA(int num_requested) override {
    VLOG(2) << "ARC app launch is requested " << num_requested << " times";
    UMA_HISTOGRAM_EXACT_LINEAR("Arc.AppRequestedInSession", num_requested, 30);
  }
};

}  // namespace

// static
ArcBootPhaseMonitorBridgeFactory*
ArcBootPhaseMonitorBridgeFactory::GetInstance() {
  return base::Singleton<ArcBootPhaseMonitorBridgeFactory>::get();
}

// static
ArcBootPhaseMonitorBridge* ArcBootPhaseMonitorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcBootPhaseMonitorBridgeFactory::GetForBrowserContext(context);
}

// static
ArcBootPhaseMonitorBridge*
ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcBootPhaseMonitorBridgeFactory::GetForBrowserContextForTesting(
      context);
}

// static
void ArcBootPhaseMonitorBridge::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kArcAppRequestedInSession, -1);
}

// static
void ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMA(
    content::BrowserContext* context) {
  auto* bridge = arc::ArcBootPhaseMonitorBridge::GetForBrowserContext(context);
  if (bridge)
    bridge->RecordFirstAppLaunchDelayUMAInternal();
}

ArcBootPhaseMonitorBridge::ArcBootPhaseMonitorBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : ArcBootPhaseMonitorBridge(
          context,
          bridge_service,
          // Set the default delegate. Unit tests may use a different one.
          std::make_unique<DefaultDelegateImpl>()) {}

ArcBootPhaseMonitorBridge::ArcBootPhaseMonitorBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service,
    std::unique_ptr<Delegate> delegate)
    : arc_bridge_service_(bridge_service),
      account_id_(multi_user_util::GetAccountIdFromProfile(
          Profile::FromBrowserContext(context))),
      delegate_(std::move(delegate)),
      pref_service_(user_prefs::UserPrefs::Get(context)) {
  arc_bridge_service_->boot_phase_monitor()->SetHost(this);
  auto* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->AddObserver(this);

  // To deal with unexpected shutdown, whether ARC App launch request
  // is made or not is stored in prefs, then processed in the next
  // user session.
  if (auto value = pref_service_->GetInteger(prefs::kArcAppRequestedInSession);
      value >= 0) {
    delegate_->RecordAppRequestedInSessionUMA(value);
  }
  pref_service_->SetInteger(prefs::kArcAppRequestedInSession, 0);
}

ArcBootPhaseMonitorBridge::~ArcBootPhaseMonitorBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->boot_phase_monitor()->SetHost(nullptr);
  auto* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->RemoveObserver(this);
}

void ArcBootPhaseMonitorBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (boot_completed_)
    observer->OnBootCompleted();
}

void ArcBootPhaseMonitorBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcBootPhaseMonitorBridge::OnBootCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "OnBootCompleted";
  boot_completed_ = true;

  ash::SessionManagerClient::Get()->EmitArcBooted(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
      base::BindOnce(&OnEmitArcBooted));

  if (!app_launch_time_.is_null() && delegate_) {
    delegate_->RecordFirstAppLaunchDelayUMA(base::TimeTicks::Now() -
                                            app_launch_time_);
  }
  for (auto& observer : observers_)
    observer.OnBootCompleted();
}

void ArcBootPhaseMonitorBridge::OnArcSessionStopped(ArcStopReason stop_reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Reset();
}

void ArcBootPhaseMonitorBridge::OnArcSessionRestarting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Reset();
}

void ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMAInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pref_service_->SetInteger(
      prefs::kArcAppRequestedInSession,
      pref_service_->GetInteger(prefs::kArcAppRequestedInSession) + 1);

  if (first_app_launch_delay_recorded_)
    return;
  first_app_launch_delay_recorded_ = true;

  if (boot_completed_) {
    VLOG(2) << "ARC has already fully started. Recording the UMA now.";
    if (delegate_)
      delegate_->RecordFirstAppLaunchDelayUMA(base::TimeDelta());
    return;
  }
  app_launch_time_ = base::TimeTicks::Now();
}

void ArcBootPhaseMonitorBridge::Reset() {
  app_launch_time_ = base::TimeTicks();
  first_app_launch_delay_recorded_ = false;
  boot_completed_ = false;
}

}  // namespace arc
