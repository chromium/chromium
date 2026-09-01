// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/server_certificate_database/server_certificate_database_service.h"

namespace net {

ServerCertificateDatabaseService*
ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ServerCertificateDatabaseService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

ServerCertificateDatabaseServiceFactory*
ServerCertificateDatabaseServiceFactory::GetInstance() {
  static base::NoDestructor<ServerCertificateDatabaseServiceFactory> instance;
  return instance.get();
}

ServerCertificateDatabaseServiceFactory::
    ServerCertificateDatabaseServiceFactory()
    : ProfileKeyedServiceFactory(
          "ServerCertificateDatabaseService",
          // Use the same service for incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // For Guest the need for these these are based off of what
              // ProfileNetworkContextService does.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // Not needed for Ash internals as it's not a real user
              // profile and so there isn't a user to use the database.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()

      ) {
}

ServerCertificateDatabaseServiceFactory::
    ~ServerCertificateDatabaseServiceFactory() = default;

std::unique_ptr<KeyedService>
ServerCertificateDatabaseServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return std::make_unique<ServerCertificateDatabaseService>(profile->GetPath());
}

}  // namespace net
