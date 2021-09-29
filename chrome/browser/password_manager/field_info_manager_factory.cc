// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/field_info_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/browser_context.h"

using password_manager::FieldInfoManager;
using password_manager::FieldInfoManagerImpl;

// static
FieldInfoManagerFactory* FieldInfoManagerFactory::GetInstance() {
  return base::Singleton<FieldInfoManagerFactory>::get();
}

// static
FieldInfoManager* FieldInfoManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FieldInfoManagerImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

FieldInfoManagerFactory::FieldInfoManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "FieldInfoManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PasswordStoreFactory::GetInstance());
}

FieldInfoManagerFactory::~FieldInfoManagerFactory() = default;

// BrowserContextKeyedServiceFactory overrides:
KeyedService* FieldInfoManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new FieldInfoManagerImpl(PasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));
}
