// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service_factory.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "chrome/browser/enterprise/net/enterprise_proxy_service_factory.h"
#endif

namespace safe_browsing {

namespace {

content::BrowserContext* ResolveContextForLookup(
    content::BrowserContext* context,
    bool use_same_instance_in_off_the_record) {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  if (use_same_instance_in_off_the_record && profile->IsOffTheRecord()) {
    return profile->GetOriginalProfile();
  }
  return profile;
}

}  // namespace

// static
NetworkContextServiceFactory* NetworkContextServiceFactory::GetInstance() {
  static base::NoDestructor<NetworkContextServiceFactory> instance;
  return instance.get();
}

// static
NetworkContextService* NetworkContextServiceFactory::GetForBrowserContext(
    content::BrowserContext* context,
    bool use_same_instance_in_off_the_record) {
  NetworkContextServiceFactory* factory = GetInstance();
  factory->AssertContextWasntDestroyed(context);
  return static_cast<NetworkContextService*>(
      factory->GetServiceForBrowserContext(
          ResolveContextForLookup(context, use_same_instance_in_off_the_record),
          /*create=*/true));
}

// static
NetworkContextService*
NetworkContextServiceFactory::GetForBrowserContextIfExistsForTesting(
    content::BrowserContext* context,
    bool use_same_instance_in_off_the_record) {
  NetworkContextServiceFactory* factory = GetInstance();
  factory->AssertContextWasntDestroyed(context);
  return static_cast<NetworkContextService*>(
      factory->GetServiceForBrowserContext(
          ResolveContextForLookup(context, use_same_instance_in_off_the_record),
          /*create=*/false));
}

NetworkContextServiceFactory::NetworkContextServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingNetworkContextService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/587263559): Tighten Guest to
              // `ProfileSelection::kOffTheRecordOnly` once all off-the-record
              // consumers stop redirecting to the Guest parent profile.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
#if BUILDFLAG(ENTERPRISE_PROXY)
  DependsOn(EnterpriseProxyServiceFactory::GetInstance());
#endif
}

NetworkContextServiceFactory::~NetworkContextServiceFactory() = default;

std::unique_ptr<KeyedService>
NetworkContextServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<NetworkContextService>(profile);
}

}  // namespace safe_browsing
