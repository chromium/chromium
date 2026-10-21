// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"


namespace chromeos {

// static
VpnServiceInterface* VpnServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<VpnServiceInterface*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
VpnServiceFactory* VpnServiceFactory::GetInstance() {
  static base::NoDestructor<VpnServiceFactory> instance;
  return instance.get();
}

VpnServiceFactory::VpnServiceFactory()
    : ProfileKeyedServiceFactory(
          "VpnService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::EventRouterFactory::GetInstance());
}

VpnServiceFactory::~VpnServiceFactory() = default;

bool VpnServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool VpnServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
VpnServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Only main profile should be allowed to access the API.
  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(context))) {
    return nullptr;
  }
  return std::make_unique<VpnService>(context);
}

}  // namespace chromeos
