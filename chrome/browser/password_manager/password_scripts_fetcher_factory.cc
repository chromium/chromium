// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"

#include "base/android/locale_utils.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/capabilities_service_impl.h"
#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"
#include "components/password_manager/core/browser/saved_passwords_capabilities_fetcher.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {

std::string GetCountryCode() {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  // Use fallback "ZZ" if no country is available.
  if (!variations_service || variations_service->GetLatestCountry().empty())
    return "ZZ";
  return base::ToUpperASCII(variations_service->GetLatestCountry());
}

}  // namespace

PasswordScriptsFetcherFactory::PasswordScriptsFetcherFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordScriptsFetcher",
          BrowserContextDependencyManager::GetInstance()) {}

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
            browser_context, chrome::GetChannel(), GetCountryCode(),
            base::android::GetDefaultLocaleString());

    std::unique_ptr<CapabilitiesServiceImpl> service =
        std::make_unique<CapabilitiesServiceImpl>(
            std::move(autofill_assistant));

    return new password_manager::SavedPasswordsCapabilitiesFetcher(
        std::move(service), PasswordStoreFactory::GetForProfile(
                                Profile::FromBrowserContext(browser_context),
                                ServiceAccessType::EXPLICIT_ACCESS));
  }

  return new password_manager::PasswordScriptsFetcherImpl(
      version_info::GetVersion(), browser_context->GetDefaultStoragePartition()
                                      ->GetURLLoaderFactoryForBrowserProcess());
}
