// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"

#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
SafeBrowsingPrivateEventRouter*
SafeBrowsingPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<SafeBrowsingPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SafeBrowsingPrivateEventRouterFactory*
SafeBrowsingPrivateEventRouterFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingPrivateEventRouterFactory> instance;
  return instance.get();
}

SafeBrowsingPrivateEventRouterFactory::SafeBrowsingPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingPrivateEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // Guest Profile follows Regular Profile selection mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  DependsOn(
      enterprise_connectors::RealtimeReportingClientFactory::GetInstance());
}

SafeBrowsingPrivateEventRouterFactory::
    ~SafeBrowsingPrivateEventRouterFactory() = default;

std::unique_ptr<KeyedService>
SafeBrowsingPrivateEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SafeBrowsingPrivateEventRouter>(context);
}

bool SafeBrowsingPrivateEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SafeBrowsingPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
