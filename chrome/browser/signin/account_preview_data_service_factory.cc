// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_preview_data_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/account_preview_data_service_impl.h"
#include "components/signin/public/base/signin_switches.h"

AccountPreviewDataServiceFactory::AccountPreviewDataServiceFactory()
    : ProfileKeyedServiceFactory("AccountPreviewDataService") {}

AccountPreviewDataServiceFactory::~AccountPreviewDataServiceFactory() = default;

// static
signin::AccountPreviewDataService*
AccountPreviewDataServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<signin::AccountPreviewDataService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccountPreviewDataServiceFactory*
AccountPreviewDataServiceFactory::GetInstance() {
  static base::NoDestructor<AccountPreviewDataServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
AccountPreviewDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(switches::kEnableAccountPreviewData)) {
    return nullptr;
  }
  return std::make_unique<signin::AccountPreviewDataServiceImpl>();
}

void AccountPreviewDataServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  signin::AccountPreviewDataService::RegisterProfilePrefs(registry);
}

bool AccountPreviewDataServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
