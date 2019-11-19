// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "EventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(extensions::EventRouterFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(VolumeManagerFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
}

EventRouterFactory::~EventRouterFactory() = default;

KeyedService* EventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new EventRouter(Profile::FromBrowserContext(context));
}

content::BrowserContext* EventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Explicitly and always allow this router in guest login mode.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool EventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace file_manager
