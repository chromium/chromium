// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service_impl.h"
#include "components/password_manager/core/browser/capabilities_service_impl.h"
#include "components/password_manager/core/browser/saved_passwords_capabilities_fetcher.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

PasswordScriptsFetcherFactory::PasswordScriptsFetcherFactory()
    : ProfileKeyedServiceFactory("PasswordScriptsFetcher") {
  DependsOn(AffiliationServiceFactory::GetInstance());
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
  Profile* profile = Profile::FromBrowserContext(browser_context);

  return new password_manager::SavedPasswordsCapabilitiesFetcher(
      std::make_unique<CapabilitiesServiceImpl>(),
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          AffiliationServiceFactory::GetForProfile(profile),
          PasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          AccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS)));
}
