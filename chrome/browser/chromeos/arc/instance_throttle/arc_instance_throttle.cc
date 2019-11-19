// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_app_launch_throttle_observer.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_boot_phase_throttle_observer.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_pip_window_throttle_observer.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"

namespace arc {

namespace {

class DefaultDelegateImpl : public ArcInstanceThrottle::Delegate {
 public:
  DefaultDelegateImpl() = default;
  ~DefaultDelegateImpl() override = default;
  void SetCpuRestriction(bool restrict) override {
    SetArcCpuRestriction(restrict);
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
                                         ArcBridgeService* bridge_service)
    : ThrottleService(context),
      delegate_(std::make_unique<DefaultDelegateImpl>()) {
  AddObserver(std::make_unique<ArcActiveWindowThrottleObserver>());
  AddObserver(std::make_unique<ArcAppLaunchThrottleObserver>());
  AddObserver(std::make_unique<ArcBootPhaseThrottleObserver>());
  AddObserver(std::make_unique<ArcPipWindowThrottleObserver>());
  StartObservers();
}

ArcInstanceThrottle::~ArcInstanceThrottle() = default;

void ArcInstanceThrottle::Shutdown() {
  StopObservers();
}

void ArcInstanceThrottle::ThrottleInstance(
    chromeos::ThrottleObserver::PriorityLevel level) {
  switch (level) {
    case chromeos::ThrottleObserver::PriorityLevel::CRITICAL:
    case chromeos::ThrottleObserver::PriorityLevel::IMPORTANT:
    case chromeos::ThrottleObserver::PriorityLevel::NORMAL:
      delegate_->SetCpuRestriction(false);
      break;
    case chromeos::ThrottleObserver::PriorityLevel::LOW:
    case chromeos::ThrottleObserver::PriorityLevel::UNKNOWN:
      delegate_->SetCpuRestriction(true);
      break;
  }
}

void ArcInstanceThrottle::RecordCpuRestrictionDisabledUMA(
    const std::string& observer_name,
    base::TimeDelta delta) {
  delegate_->RecordCpuRestrictionDisabledUMA(observer_name, delta);
}

}  // namespace arc
