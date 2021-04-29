// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/change_password_url_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/change_password_url_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

ChangePasswordUrlServiceFactory::ChangePasswordUrlServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ChangePasswordUrlService",
          BrowserContextDependencyManager::GetInstance()) {}

ChangePasswordUrlServiceFactory::~ChangePasswordUrlServiceFactory() = default;

// static
ChangePasswordUrlServiceFactory*
ChangePasswordUrlServiceFactory::GetInstance() {
  static base::NoDestructor<ChangePasswordUrlServiceFactory> instance;
  return instance.get();
}

// static
password_manager::ChangePasswordUrlService*
ChangePasswordUrlServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<password_manager::ChangePasswordUrlService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

KeyedService* ChangePasswordUrlServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new password_manager::ChangePasswordUrlServiceImpl(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      Profile::FromBrowserContext(context)->GetPrefs());
}
