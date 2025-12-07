// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/account_setting_service_factory.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

namespace autofill {

// static
AccountSettingServiceFactory* AccountSettingServiceFactory::GetInstance() {
  static base::NoDestructor<AccountSettingServiceFactory> instance;
  return instance.get();
}

// static
AccountSettingService* AccountSettingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountSettingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AccountSettingServiceFactory::AccountSettingServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccountSettingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AccountSettingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AccountSettingService>(
      base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)
          ? std::make_unique<AccountSettingSyncBridge>(
                std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
                    syncer::ACCOUNT_SETTING,
                    /*dump_stack=*/base::DoNothing()),
                DataTypeStoreServiceFactory::GetForProfile(profile)
                    ->GetStoreFactory())
          : nullptr);
}

}  // namespace autofill
