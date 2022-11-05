// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace file_manager {

// static
EventRouter* EventRouterFactory::GetForProfile(Profile* profile) {
  return static_cast<EventRouter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EventRouterFactory* EventRouterFactory::GetInstance() {
  return base::Singleton<EventRouterFactory>::get();
}

EventRouterFactory::EventRouterFactory()
    : ProfileKeyedServiceFactory(
          "EventRouter",
          // Explicitly and always allow this router in guest login mode.
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(extensions::EventRouterFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(VolumeManagerFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

EventRouterFactory::~EventRouterFactory() = default;

KeyedService* EventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new EventRouter(Profile::FromBrowserContext(context));
}

bool EventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace file_manager
