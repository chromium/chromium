// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {
namespace {

void OnEmitArcBooted(bool success) {
  if (!success)
    VLOG(1) << "Failed to emit arc booted signal.";
}

class DefaultDelegateImpl : public ArcBootPhaseMonitorBridge::Delegate {
 public:
  DefaultDelegateImpl() = default;
  ~DefaultDelegateImpl() override = default;

  void RecordFirstAppLaunchDelayUMA(base::TimeDelta delta) override {
    VLOG(2) << "Launching the first app took "
            << delta.InMillisecondsRoundedUp() << " ms.";
    UMA_HISTOGRAM_CUSTOM_TIMES("Arc.FirstAppLaunchDelay.TimeDelta", delta,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(2), 50);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegateImpl);
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
void ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMA(
    content::BrowserContext* context) {
  auto* bridge = arc::ArcBootPhaseMonitorBridge::GetForBrowserContext(context);
  if (bridge)
    bridge->RecordFirstAppLaunchDelayUMAInternal();
}

ArcBootPhaseMonitorBridge::ArcBootPhaseMonitorBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      account_id_(multi_user_util::GetAccountIdFromProfile(
          Profile::FromBrowserContext(context))),
      // Set the default delegate. Unit tests may use a different one.
      delegate_(std::make_unique<DefaultDelegateImpl>()) {
  arc_bridge_service_->boot_phase_monitor()->SetHost(this);
  auto* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->AddObserver(this);
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
}

void ArcBootPhaseMonitorBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMAInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void ArcBootPhaseMonitorBridge::OnBootCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "OnBootCompleted";
  boot_completed_ = true;

  chromeos::SessionManagerClient::Get()->EmitArcBooted(
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

void ArcBootPhaseMonitorBridge::Reset() {
  app_launch_time_ = base::TimeTicks();
  first_app_launch_delay_recorded_ = false;
  boot_completed_ = false;
}

void ArcBootPhaseMonitorBridge::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

}  // namespace arc
