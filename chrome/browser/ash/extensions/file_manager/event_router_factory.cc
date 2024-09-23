// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
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
  static base::NoDestructor<EventRouterFactory> instance;
  return instance.get();
}

EventRouterFactory::EventRouterFactory()
    : ProfileKeyedServiceFactory(
          "EventRouter",
          // Explicitly and always allow this router in guest login mode.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(extensions::EventRouterFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(VolumeManagerFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(guest_os::GuestOsServiceFactory::GetInstance());
  DependsOn(policy::DlpRulesManagerFactory::GetInstance());
}

EventRouterFactory::~EventRouterFactory() = default;

std::unique_ptr<KeyedService>
EventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<EventRouter>(Profile::FromBrowserContext(context));
}

bool EventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace file_manager
