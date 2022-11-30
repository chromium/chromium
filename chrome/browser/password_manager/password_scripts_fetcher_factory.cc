// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/password_manager/core/browser/capabilities_service_impl.h"
#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"
#include "components/password_manager/core/browser/saved_passwords_capabilities_fetcher.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

PasswordScriptsFetcherFactory::PasswordScriptsFetcherFactory()
    : ProfileKeyedServiceFactory("PasswordScriptsFetcher") {
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
}

PasswordScriptsFetcherFactory::~PasswordScriptsFetcherFactory() = default;

// static
PasswordScriptsFetcherFactory* PasswordScriptsFetcherFactory::GetInstance() {
  static base::NoDestructor<PasswordScriptsFetcherFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordScriptsFetcher*
PasswordScriptsFetcherFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<password_manager::PasswordScriptsFetcher*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

KeyedService* PasswordScriptsFetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordDomainCapabilitiesFetching)) {
    std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant =
        autofill_assistant::AutofillAssistantFactory::CreateForBrowserContext(
            browser_context,
            std::make_unique<autofill_assistant::CommonDependenciesChrome>(
                browser_context));

    std::unique_ptr<CapabilitiesServiceImpl> service =
        std::make_unique<CapabilitiesServiceImpl>(
            std::move(autofill_assistant));

    Profile* profile = Profile::FromBrowserContext(browser_context);
    password_manager::AffiliationService* affiliation_service =
        AffiliationServiceFactory::GetForProfile(profile);

    return new password_manager::SavedPasswordsCapabilitiesFetcher(
        std::move(service),
        std::make_unique<password_manager::SavedPasswordsPresenter>(
            affiliation_service,
            PasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::EXPLICIT_ACCESS),
            AccountPasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::EXPLICIT_ACCESS)));
  }

  return new password_manager::PasswordScriptsFetcherImpl(
      std::make_unique<autofill_assistant::CommonDependenciesChrome>(
          browser_context)
          ->IsSupervisedUser(),
      version_info::GetVersion(),
      browser_context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
