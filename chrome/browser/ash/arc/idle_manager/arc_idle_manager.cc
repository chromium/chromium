// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_idle_manager.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_cpu_throttle_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_display_power_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_on_battery_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"

namespace arc {

namespace {

class DefaultDelegateImpl : public ArcIdleManager::Delegate {
 public:
  DefaultDelegateImpl() = default;

  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;

  ~DefaultDelegateImpl() override = default;

  // ArcIdleManager::Delegate:
  void SetInteractiveMode(ArcPowerBridge* arc_power_bridge,
                          ArcBridgeService* bridge,
                          bool enable) override {
    if (!arc_power_bridge) {
      return;
    }
    arc_power_bridge->NotifyAndroidInteractiveState(bridge, enable);
  }
};

// Singleton factory for ArcIdleManager.
class ArcIdleManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcIdleManager,
          ArcIdleManagerFactory> {
 public:
  static constexpr const char* kName = "ArcIdleManagerFactory";
  static ArcIdleManagerFactory* GetInstance() {
    static base::NoDestructor<ArcIdleManagerFactory> instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<ArcIdleManagerFactory>;

  ArcIdleManagerFactory() { DependsOn(ArcPowerBridgeFactory::GetInstance()); }
  ~ArcIdleManagerFactory() override = default;
};

}  // namespace

// static
ArcIdleManager* ArcIdleManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcIdleManagerFactory::GetForBrowserContext(context);
}

// static
ArcIdleManager* ArcIdleManager::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcIdleManagerFactory::GetForBrowserContextForTesting(context);
}

ArcIdleManager::ArcIdleManager(content::BrowserContext* context,
                               ArcBridgeService* bridge)
    : ThrottleService(context),
      delegate_(std::make_unique<DefaultDelegateImpl>()),
      bridge_(bridge) {
  AddObserver(std::make_unique<ArcCpuThrottleObserver>());
  AddObserver(std::make_unique<ArcBackgroundServiceObserver>());
  AddObserver(std::make_unique<ArcWindowObserver>());
  if (kEnableArcIdleManagerIgnoreBatteryForPLT.Get()) {
    LOG(WARNING) << "Doze will be enabled regardless of battery status";
  } else {
    AddObserver(std::make_unique<ArcOnBatteryObserver>());
  }
  AddObserver(std::make_unique<ArcDisplayPowerObserver>());

  arc_power_bridge_ = ArcPowerBridge::GetForBrowserContext(context);

  // This maybe null in unit tests.
  if (arc_power_bridge_) {
    arc_power_bridge_->DisableAndroidIdleControl();
    powerbridge_observation_.Observe(arc_power_bridge_);
  }

  DCHECK(bridge_);
  bridge_->power()->AddObserver(this);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// Destructor is empty because this is a KeyedService, which must
// cleanup in Shutdown();
ArcIdleManager::~ArcIdleManager() = default;

// static
void ArcIdleManager::EnsureFactoryBuilt() {
  ArcIdleManagerFactory::GetInstance();
}

void ArcIdleManager::Shutdown() {
  // After this is done, we will no longer get connection notifications.
  bridge_->power()->RemoveObserver(this);

  // No more notifications about VM resumed.
  powerbridge_observation_.Reset();

  // Safeguard against resource leak by observers.
  OnConnectionClosed();
}

void ArcIdleManager::OnConnectionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_connected_)
    return;
  StartObservers();
  delegate_->SetInteractiveMode(arc_power_bridge_, bridge_, !should_throttle());
  is_connected_ = true;

  // Always reset the timer on connect.
  LogScreenOffTimer(/*toggle_timer*/ true);
  // Next call to LogScreenOffTimer from ThrottleInstance will either:
  //   a) throttle=true: reset the timer again - and that's fine.
  //   b) throttle=false: log time between connect and un-throttle.
}

void ArcIdleManager::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_connected_)
    return;
  StopObservers();
  if (should_throttle()) {
    // Maybe a logout, or a systemserver crash.
    // Either way, we stop tracking and log.
    LogScreenOffTimer(/*toggle_timer*/ false);
  }
  is_connected_ = false;
}

void ArcIdleManager::ThrottleInstance(bool should_throttle) {
  // Note: this never happens in between StopObservers() - StartObservers();
  if (!first_idle_happened_ && !should_throttle) {
    // Both the ArcIdleManager and Android start life as un-throttled (not
    // idle). Until it's time to throttle Android, the state is aligned, and
    // there's no need to send requests to change state.
    return;
  }
  first_idle_happened_ = true;
  LogScreenOffTimer(/*toggle_timer*/ should_throttle);
  delegate_->SetInteractiveMode(arc_power_bridge_, bridge_, !should_throttle);
}

void ArcIdleManager::OnVmResumed() {
  if (!should_throttle()) {
    // A resume happens because there was a prior suspend.
    // That earlier suspend counts as first-idle.
    first_idle_happened_ = true;

    // Just sync up Android state with internal state.
    // No need for logging metrics, not a state change.
    delegate_->SetInteractiveMode(arc_power_bridge_, bridge_, true);
  }
}

void ArcIdleManager::OnWillDestroyArcPowerBridge() {
  // No more notifications about VM resumed.
  powerbridge_observation_.Reset();
  arc_power_bridge_ = nullptr;
}

void ArcIdleManager::LogScreenOffTimer(bool toggle_timer) {
  if (toggle_timer) {
    // Start measuring now.
    interactive_off_span_timer_ = base::ElapsedTimer();
  } else {
    base::TimeDelta elapsed = interactive_off_span_timer_.Elapsed();
    // Report time spent with screen-off, in milliseconds. Use 100 buckets,
    // as the span of allowed values is very wide (1ms -> 8h(28,800,000ms)).
    // Notice that the very first call to this function may hit this case,
    // which will cause us to log the time between start-up and the
    // transition to no-throttle (first-active), which is an appropriate
    // measurement value.
    base::UmaHistogramCustomTimes("Arc.IdleManager.ScreenOffTime",
                                  /*sample=*/elapsed,
                                  /*min=*/base::Milliseconds(1),
                                  /*max=*/base::Hours(8), /*buckets=*/100);
  }
}

}  // namespace arc
