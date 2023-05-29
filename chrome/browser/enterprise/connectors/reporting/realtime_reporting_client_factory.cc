// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"

#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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
  static base::NoDestructor<RealtimeReportingClientFactory> instance;
  return instance.get();
}

RealtimeReportingClientFactory::RealtimeReportingClientFactory()
    : ProfileKeyedServiceFactory(
          "RealtimeReportingClient",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // Guest Profile follows Regular Profile selection mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ConnectorsServiceFactory::GetInstance());
}

RealtimeReportingClientFactory::~RealtimeReportingClientFactory() = default;

KeyedService* RealtimeReportingClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RealtimeReportingClient(context);
}

bool RealtimeReportingClientFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

bool RealtimeReportingClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace enterprise_connectors
