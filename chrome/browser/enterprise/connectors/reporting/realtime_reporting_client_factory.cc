// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"

#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace enterprise_connectors {

// static
RealtimeReportingClient* RealtimeReportingClientFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<RealtimeReportingClient*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RealtimeReportingClientFactory* RealtimeReportingClientFactory::GetInstance() {
  return base::Singleton<RealtimeReportingClientFactory>::get();
}

RealtimeReportingClientFactory::RealtimeReportingClientFactory()
    : BrowserContextKeyedServiceFactory(
          "RealtimeReportingClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
}

RealtimeReportingClientFactory::~RealtimeReportingClientFactory() = default;

KeyedService* RealtimeReportingClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RealtimeReportingClient(context);
}

content::BrowserContext* RealtimeReportingClientFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsSystemProfile()) {
    return nullptr;
  }
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

bool RealtimeReportingClientFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

bool RealtimeReportingClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace enterprise_connectors
