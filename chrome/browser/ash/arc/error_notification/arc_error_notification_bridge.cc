// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/error_notification/arc_error_notification_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/singleton.h"

namespace arc {

namespace {

// Singleton factory for ArcErrorNotificationBridgeFactory.
class ArcErrorNotificationBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcErrorNotificationBridge,
          ArcErrorNotificationBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcErrorNotificationBridgeFactory";

  static ArcErrorNotificationBridgeFactory* GetInstance() {
    return base::Singleton<ArcErrorNotificationBridgeFactory>::get();
  }

  ArcErrorNotificationBridgeFactory() = default;
  ~ArcErrorNotificationBridgeFactory() override = default;
};

}  // namespace

// static
ArcErrorNotificationBridge* ArcErrorNotificationBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcErrorNotificationBridgeFactory::GetForBrowserContext(context);
}

// static
void ArcErrorNotificationBridge::EnsureFactoryBuilt() {
  ArcErrorNotificationBridgeFactory::GetInstance();
}

ArcErrorNotificationBridge::ArcErrorNotificationBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->error_notification()->SetHost(this);
}

ArcErrorNotificationBridge::~ArcErrorNotificationBridge() {
  arc_bridge_service_->error_notification()->SetHost(nullptr);
}

void ArcErrorNotificationBridge::SendErrorDetails(
    mojom::ErrorDetailsPtr details,
    mojo::PendingRemote<mojom::ErrorNotificationActionHandler> action_handler,
    SendErrorDetailsCallback callback) {
  // TODO(b/332459217): Add implementation.
  std::move(callback).Run(mojo::NullRemote());
}

}  // namespace arc
