// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/autofill/content/browser/content_autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/command_line_switches.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/storage_partition.h"

namespace autofill {

namespace {

// Return the latest country code from the chrome variation service.
// If the variation service is not available, an empty string is returned.
const std::string GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}
}  // namespace

// static
PersonalDataManager* PersonalDataManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PersonalDataManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PersonalDataManagerFactory* PersonalDataManagerFactory::GetInstance() {
  static base::NoDestructor<PersonalDataManagerFactory> instance;
  return instance.get();
}

PersonalDataManagerFactory::PersonalDataManagerFactory()
    : ProfileKeyedServiceFactory(
          "PersonalDataManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(StrikeDatabaseFactory::GetInstance());
  DependsOn(AutofillImageFetcherFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() = default;

std::unique_ptr<KeyedService>
PersonalDataManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
 Profile* profile = Profile::FromBrowserContext(context);
  // WebDataServiceFactory redirects to the original profile.
  auto local_storage = WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto account_storage = WebDataServiceFactory::GetAutofillWebDataForAccount(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  // The HistoryServiceFactory redirects to the original profile.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  auto* strike_database = StrikeDatabaseFactory::GetForProfile(profile);

  // The AutofillImageFetcherFactory redirects to the original profile.
  auto* image_fetcher = AutofillImageFetcherFactory::GetForProfile(profile);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  auto* sync_service = SyncServiceFactory::GetForProfile(profile);

  auto* shared_storage_manager =
      profile->GetDefaultStoragePartition()->GetSharedStorageManager();
  auto shared_storage_handler =
      shared_storage_manager
          ? std::make_unique<ContentAutofillSharedStorageHandler>(
                *shared_storage_manager)
          : nullptr;

  return std::make_unique<PersonalDataManager>(
      local_storage, account_storage, profile->GetPrefs(),
      g_browser_process->local_state(), identity_manager, history_service,
      sync_service, strike_database, image_fetcher,
      std::move(shared_storage_handler),
      g_browser_process->GetApplicationLocale(),
      GetCountryCodeFromVariations());
}

}  // namespace autofill
