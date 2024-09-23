// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service.h"

namespace safe_browsing {

// static
NetworkContextServiceFactory* NetworkContextServiceFactory::GetInstance() {
  static base::NoDestructor<NetworkContextServiceFactory> instance;
  return instance.get();
}

// static
NetworkContextService* NetworkContextServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<NetworkContextService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

NetworkContextServiceFactory::NetworkContextServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingNetworkContextService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

NetworkContextServiceFactory::~NetworkContextServiceFactory() = default;

std::unique_ptr<KeyedService>
NetworkContextServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<NetworkContextService>(profile);
}

}  // namespace safe_browsing
