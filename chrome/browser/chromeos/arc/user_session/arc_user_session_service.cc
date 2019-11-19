// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/user_session/arc_user_session_service.h"

#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/session_manager/core/session_manager.h"

namespace arc {
namespace {

// Singleton factory for ArcUserSessionService.
class ArcUserSessionServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcUserSessionService,
          ArcUserSessionServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcUserSessionServiceFactory";

  static ArcUserSessionServiceFactory* GetInstance() {
    return base::Singleton<ArcUserSessionServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcUserSessionServiceFactory>;
  ArcUserSessionServiceFactory() = default;
  ~ArcUserSessionServiceFactory() override = default;
};

}  // namespace

ArcUserSessionService* ArcUserSessionService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcUserSessionServiceFactory::GetForBrowserContext(context);
}

ArcUserSessionService::ArcUserSessionService(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->intent_helper()->AddObserver(this);
}

ArcUserSessionService::~ArcUserSessionService() {
  // OnConnectionClosed() is not guaranteed to be called before destruction.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  arc_bridge_service_->intent_helper()->RemoveObserver(this);
}

void ArcUserSessionService::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state != session_manager::SessionState::ACTIVE)
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->intent_helper(), SendBroadcast);
  if (!instance)
    return;

  instance->SendBroadcast(
      ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
          "USER_SESSION_ACTIVE"),
      ArcIntentHelperBridge::kArcIntentHelperPackageName,
      ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
          "ArcIntentHelperService"),
      "{}");
}

void ArcUserSessionService::OnConnectionReady() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

void ArcUserSessionService::OnConnectionClosed() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

}  // namespace arc
