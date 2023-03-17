// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "content/public/browser/storage_partition.h"

// static
KidsChromeManagementClient* KidsChromeManagementClientFactory::GetForProfile(
    Profile* profile) {
  return static_cast<KidsChromeManagementClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
KidsChromeManagementClientFactory*
KidsChromeManagementClientFactory::GetInstance() {
  static base::NoDestructor<KidsChromeManagementClientFactory> factory;
  return factory.get();
}

KidsChromeManagementClientFactory::KidsChromeManagementClientFactory()
    : ProfileKeyedServiceFactory(
          "KidsChromeManagementClient",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KidsChromeManagementClientFactory::~KidsChromeManagementClientFactory() =
    default;

KeyedService* KidsChromeManagementClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new KidsChromeManagementClient(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      IdentityManagerFactory::GetForProfile(profile));
}
