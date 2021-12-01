// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_kiosk_mode_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_pip_window_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_provisioning_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_switch_throttle_observer.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace arc {

namespace {

void OnSetArcVmCpuRestriction(
    absl::optional<vm_tools::concierge::SetVmCpuRestrictionResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call SetVmCpuRestriction";
    return;
  }
  if (!response->success())
    LOG(ERROR) << "SetVmCpuRestriction for ARCVM failed";
}

void SetArcVmCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  auto* const client = chromeos::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "ConciergeClient is not available";
    return;
  }

  vm_tools::concierge::SetVmCpuRestrictionRequest request;
  request.set_cpu_cgroup(vm_tools::concierge::CPU_CGROUP_ARCVM);
  switch (cpu_restriction_state) {
    case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
      request.set_cpu_restriction_state(
          vm_tools::concierge::CPU_RESTRICTION_FOREGROUND);
      break;
    case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
      request.set_cpu_restriction_state(
          vm_tools::concierge::CPU_RESTRICTION_BACKGROUND);
      break;
  }

  client->SetVmCpuRestriction(request,
                              base::BindOnce(&OnSetArcVmCpuRestriction));
}

void SetArcCpuRestrictionCallback(
    login_manager::ContainerCpuRestrictionState state,
    bool success) {
  if (success)
    return;
  const char* message =
      (state == login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND)
          ? "unprioritize"
          : "prioritize";
  LOG(ERROR) << "Failed to " << message << " ARC";
}

void SetArcContainerCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  if (!chromeos::SessionManagerClient::Get()) {
    LOG(WARNING) << "SessionManagerClient is not available";
    return;
  }

  login_manager::ContainerCpuRestrictionState state;
  switch (cpu_restriction_state) {
    case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
      state = login_manager::CONTAINER_CPU_RESTRICTION_FOREGROUND;
      break;
    case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
      state = login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND;
      break;
  }
  chromeos::SessionManagerClient::Get()->SetArcCpuRestriction(
      state, base::BindOnce(SetArcCpuRestrictionCallback, state));
}

// Adjusts the amount of CPU the ARC instance is allowed to use. When
// |cpu_restriction_state| is CPU_RESTRICTION_BACKGROUND, the limit is adjusted
// so ARC can only use tightly restricted CPU resources.
void SetArcCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  if (IsArcVmEnabled())
    SetArcVmCpuRestriction(cpu_restriction_state);
  else
    SetArcContainerCpuRestriction(cpu_restriction_state);
}

class DefaultDelegateImpl : public ArcInstanceThrottle::Delegate {
 public:
  DefaultDelegateImpl() = default;

  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;

  ~DefaultDelegateImpl() override = default;
  void SetCpuRestriction(CpuRestrictionState cpu_restriction_state) override {
    SetArcCpuRestriction(cpu_restriction_state);
  }

  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override {
    DVLOG(2) << "ARC throttling was disabled for "
             << delta.InMillisecondsRoundedUp()
             << " ms due to: " << observer_name;
    UmaHistogramLongTimes("Arc.CpuRestrictionDisabled." + observer_name, delta);
  }
};

// Singleton factory for ArcInstanceThrottle.
class ArcInstanceThrottleFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInstanceThrottle,
          ArcInstanceThrottleFactory> {
 public:
  static constexpr const char* kName = "ArcInstanceThrottleFactory";
  static ArcInstanceThrottleFactory* GetInstance() {
    static base::NoDestructor<ArcInstanceThrottleFactory> instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<ArcInstanceThrottleFactory>;

  ArcInstanceThrottleFactory() {
    DependsOn(ArcBootPhaseMonitorBridgeFactory::GetInstance());
  }
  ~ArcInstanceThrottleFactory() override = default;
};

CpuRestrictionState LevelToCpuRestriction(
    ash::ThrottleObserver::PriorityLevel level) {
  switch (level) {
    case ash::ThrottleObserver::PriorityLevel::CRITICAL:
    case ash::ThrottleObserver::PriorityLevel::IMPORTANT:
    case ash::ThrottleObserver::PriorityLevel::NORMAL:
      return CpuRestrictionState::CPU_RESTRICTION_FOREGROUND;
    case ash::ThrottleObserver::PriorityLevel::LOW:
    case ash::ThrottleObserver::PriorityLevel::UNKNOWN:
      return CpuRestrictionState::CPU_RESTRICTION_BACKGROUND;
  }
}

}  // namespace

// static
ArcInstanceThrottle* ArcInstanceThrottle::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInstanceThrottleFactory::GetForBrowserContext(context);
}

// static
ArcInstanceThrottle* ArcInstanceThrottle::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInstanceThrottleFactory::GetForBrowserContextForTesting(context);
}

ArcInstanceThrottle::ArcInstanceThrottle(content::BrowserContext* context,
                                         ArcBridgeService* bridge)
    : ThrottleService(context),
      delegate_(std::make_unique<DefaultDelegateImpl>()),
      bridge_(bridge) {
  AddObserver(std::make_unique<ArcActiveWindowThrottleObserver>());
  AddObserver(std::make_unique<ArcAppLaunchThrottleObserver>());
  AddObserver(std::make_unique<ArcBootPhaseThrottleObserver>());
  AddObserver(std::make_unique<ArcKioskModeThrottleObserver>());
  AddObserver(std::make_unique<ArcPipWindowThrottleObserver>());
  AddObserver(std::make_unique<ArcPowerThrottleObserver>());
  AddObserver(std::make_unique<ArcProvisioningThrottleObserver>());
  AddObserver(std::make_unique<ArcSwitchThrottleObserver>());
  StartObservers();
  DCHECK(bridge_);
  bridge_->power()->AddObserver(this);
}

ArcInstanceThrottle::~ArcInstanceThrottle() = default;

void ArcInstanceThrottle::Shutdown() {
  bridge_->power()->RemoveObserver(this);

  StopObservers();
}

void ArcInstanceThrottle::OnConnectionReady() {
  NotifyCpuRestriction(LevelToCpuRestriction(level()));
}

void ArcInstanceThrottle::ThrottleInstance(
    ash::ThrottleObserver::PriorityLevel level) {
  const CpuRestrictionState cpu_restriction_state =
      LevelToCpuRestriction(level);
  NotifyCpuRestriction(cpu_restriction_state);
  delegate_->SetCpuRestriction(cpu_restriction_state);
}

void ArcInstanceThrottle::RecordCpuRestrictionDisabledUMA(
    const std::string& observer_name,
    base::TimeDelta delta) {
  delegate_->RecordCpuRestrictionDisabledUMA(observer_name, delta);
}

void ArcInstanceThrottle::NotifyCpuRestriction(
    CpuRestrictionState cpu_restriction_state) {
  if (!base::FeatureList::IsEnabled(kEnableThrottlingNotification))
    return;

  auto* power =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_->power(), OnCpuRestrictionChanged);
  if (!power)
    return;
  power->OnCpuRestrictionChanged(
      static_cast<mojom::CpuRestrictionState>(cpu_restriction_state));
}

}  // namespace arc
