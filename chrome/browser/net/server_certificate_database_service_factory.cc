// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

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
          base::FeatureList::IsEnabled(
              ::features::kEnableCertManagementUIV2Write)
              ?
              // Use the same service for incognito profiles.
              ProfileSelections::Builder()
                  .WithRegular(ProfileSelection::kRedirectedToOriginal)
                  // For Guest and Ash internals, the need for these these are
                  // based off of what ProfileNetworkContextService does.
                  .WithGuest(ProfileSelection::kRedirectedToOriginal)
                  .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
                  .Build()
              : ProfileSelections::BuildNoProfilesSelected()

      ) {}

ServerCertificateDatabaseServiceFactory::
    ~ServerCertificateDatabaseServiceFactory() = default;

std::unique_ptr<KeyedService>
ServerCertificateDatabaseServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ServerCertificateDatabaseService>(
      profile->GetPath(), profile->GetPrefs(),
      NssServiceFactory::GetForContext(profile)
          ->CreateNSSCertDatabaseGetterForIOThread());
#else
  return std::make_unique<ServerCertificateDatabaseService>(profile->GetPath());
#endif
}

}  // namespace net
