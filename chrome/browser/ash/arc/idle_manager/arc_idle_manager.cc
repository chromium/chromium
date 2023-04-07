// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_idle_manager.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_cpu_throttle_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_display_power_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_on_battery_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"

namespace arc {

namespace {

class DefaultDelegateImpl : public ArcIdleManager::Delegate {
 public:
  DefaultDelegateImpl() = default;

  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;

  ~DefaultDelegateImpl() override = default;

  // ArcIdleManager::Delegate:
  void SetInteractiveMode(ArcBridgeService* bridge, bool enable) override {
    auto* const power =
        ARC_GET_INSTANCE_FOR_METHOD(bridge->power(), SetInteractive);
    if (!power)
      return;
    // When enable=false,
    // the code below is equivalent to pressing the power button on
    // a smartphone, which turns its screen off and kicks off a gradual
    // power state transition, ultimately leading to doze mode.
    power->SetInteractive(enable);
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

  ArcIdleManagerFactory() = default;
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

  auto* const power_bridge = ArcPowerBridge::GetForBrowserContext(context);

  // This may be null in unit tests.
  if (power_bridge)
    power_bridge->DisableAndroidIdleControl();

  DCHECK(bridge_);
  bridge_->power()->AddObserver(this);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// Destructor is empty because this is a KeyedService, which must
// cleanup in Shutdown();
ArcIdleManager::~ArcIdleManager() = default;

void ArcIdleManager::Shutdown() {
  // After this is done, we will no longer get connection notifications.
  bridge_->power()->RemoveObserver(this);

  // Safeguard against resource leak by observers.
  OnConnectionClosed();
}

void ArcIdleManager::OnConnectionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_connected_)
    return;
  StartObservers();
  delegate_->SetInteractiveMode(bridge_, !should_throttle());
  is_connected_ = true;
}

void ArcIdleManager::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_connected_)
    return;
  StopObservers();
  is_connected_ = false;
}

void ArcIdleManager::ThrottleInstance(bool should_throttle) {
  delegate_->SetInteractiveMode(bridge_, !should_throttle);
}

}  // namespace arc
