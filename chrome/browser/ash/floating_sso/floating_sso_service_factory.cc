// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"

#include <memory>

#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace ash::floating_sso {

// static
FloatingSsoService* FloatingSsoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FloatingSsoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FloatingSsoServiceFactory* FloatingSsoServiceFactory::GetInstance() {
  static base::NoDestructor<FloatingSsoServiceFactory> instance;
  return instance.get();
}

FloatingSsoServiceFactory::FloatingSsoServiceFactory()
    : ProfileKeyedServiceFactory(
          "FloatingSsoService",
          ProfileSelections::Builder()
              // Floating SSO is about syncing cookies between ChromeOS devices
              // which only makes sense for regular user profiles.
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

FloatingSsoServiceFactory::~FloatingSsoServiceFactory() = default;

std::unique_ptr<KeyedService>
FloatingSsoServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  return std::make_unique<FloatingSsoService>(
      prefs,
      std::make_unique<FloatingSsoSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::COOKIES,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel())),
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory()),
      cookie_manager);
}

bool FloatingSsoServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace ash::floating_sso
