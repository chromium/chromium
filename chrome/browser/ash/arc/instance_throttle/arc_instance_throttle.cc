// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_pip_window_throttle_observer.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/mojom/power.mojom.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {

namespace {

class DefaultDelegateImpl : public ArcInstanceThrottle::Delegate {
 public:
  DefaultDelegateImpl() = default;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegateImpl);
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
    chromeos::ThrottleObserver::PriorityLevel level) {
  switch (level) {
    case chromeos::ThrottleObserver::PriorityLevel::CRITICAL:
    case chromeos::ThrottleObserver::PriorityLevel::IMPORTANT:
    case chromeos::ThrottleObserver::PriorityLevel::NORMAL:
      return CpuRestrictionState::CPU_RESTRICTION_FOREGROUND;
    case chromeos::ThrottleObserver::PriorityLevel::LOW:
    case chromeos::ThrottleObserver::PriorityLevel::UNKNOWN:
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
  AddObserver(std::make_unique<ArcPipWindowThrottleObserver>());
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
    chromeos::ThrottleObserver::PriorityLevel level) {
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
