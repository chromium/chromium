// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

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
  return new password_manager::PasswordScriptsFetcherImpl(
      version_info::GetVersion(), browser_context->GetDefaultStoragePartition()
                                      ->GetURLLoaderFactoryForBrowserProcess());
}
