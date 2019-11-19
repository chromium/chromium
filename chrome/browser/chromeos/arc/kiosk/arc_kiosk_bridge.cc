// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/kiosk/arc_kiosk_bridge.h"

#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service_factory.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/user_manager/user_manager.h"

namespace arc {
namespace {

// Singleton factory for ArcKioskBridge.
class ArcKioskBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcKioskBridge,
          ArcKioskBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcKioskBridgeFactory";

  static ArcKioskBridgeFactory* GetInstance() {
    return base::Singleton<ArcKioskBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcKioskBridgeFactory>;

  ArcKioskBridgeFactory() {
    DependsOn(chromeos::ArcKioskAppServiceFactory::GetInstance());
  }
  ~ArcKioskBridgeFactory() override = default;

  // BrowserContextKeyedServiceFactory override:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    // Kiosk apps should be run only for kiosk sessions.
    if (!user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp())
      return nullptr;
    return ArcBrowserContextKeyedServiceFactoryBase::BuildServiceInstanceFor(
        context);
  }
};

}  // namespace

// static
ArcKioskBridge* ArcKioskBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcKioskBridgeFactory::GetForBrowserContext(context);
}

// static
std::unique_ptr<ArcKioskBridge> ArcKioskBridge::CreateForTesting(
    ArcBridgeService* arc_bridge_service,
    Delegate* delegate) {
  // std::make_unique() cannot be used because the ctor is private.
  return base::WrapUnique(new ArcKioskBridge(arc_bridge_service, delegate));
}

ArcKioskBridge::ArcKioskBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : ArcKioskBridge(bridge_service,
                     chromeos::ArcKioskAppService::Get(context)) {}

ArcKioskBridge::ArcKioskBridge(ArcBridgeService* bridge_service,
                               Delegate* delegate)
    : arc_bridge_service_(bridge_service), delegate_(delegate) {
  DCHECK(delegate_);
  arc_bridge_service_->kiosk()->SetHost(this);
}

ArcKioskBridge::~ArcKioskBridge() {
  arc_bridge_service_->kiosk()->SetHost(nullptr);
}

void ArcKioskBridge::OnMaintenanceSessionCreated(int32_t session_id) {
  session_id_ = session_id;
  delegate_->OnMaintenanceSessionCreated();
  // TODO(poromov@) Show appropriate splash screen.
}

void ArcKioskBridge::OnMaintenanceSessionFinished(int32_t session_id,
                                                  bool success) {
  // Filter only callbacks for the started kiosk session.
  if (session_id != session_id_)
    return;
  session_id_ = -1;
  delegate_->OnMaintenanceSessionFinished();
}

}  // namespace arc
