// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

namespace {

// Singleton factory for ArcSystemStateBridgeFactory.
class ArcSystemStateBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSystemStateBridge,
          ArcSystemStateBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSystemStateBridgeFactory";

  static ArcSystemStateBridgeFactory* GetInstance() {
    static base::NoDestructor<ArcSystemStateBridgeFactory> factory;
    return factory.get();
  }

  ArcSystemStateBridgeFactory() = default;
  ~ArcSystemStateBridgeFactory() override = default;
};

}  // namespace

// static
ArcSystemStateBridge* ArcSystemStateBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSystemStateBridgeFactory::GetForBrowserContext(context);
}

ArcSystemStateBridge* ArcSystemStateBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcSystemStateBridgeFactory::GetForBrowserContextForTesting(context);
}

// static
void ArcSystemStateBridge::EnsureFactoryBuilt() {
  ArcSystemStateBridgeFactory::GetInstance();
}

ArcSystemStateBridge::ArcSystemStateBridge(content::BrowserContext* context,
                                           ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      profile_(Profile::FromBrowserContext(context)) {
  arc_bridge_service_->system_state()->SetHost(this);
  DVLOG(2) << "ArcSystemStateBridge created";
}

ArcSystemStateBridge::~ArcSystemStateBridge() {
  arc_bridge_service_->system_state()->SetHost(nullptr);
}

void ArcSystemStateBridge::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcSystemStateBridge::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcSystemStateBridge::UpdateAppRunningState(
    mojom::SystemAppRunningStatePtr state) {
  if (!state.Equals(state_)) {
    state_ = state.Clone();

    for (auto& observer : observer_list_) {
      observer.OnArcSystemAppRunningStateChange(*state_);
    }
  }
}

}  // namespace arc
