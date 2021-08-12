// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/profile_account_manager_factory.h"

#include "chrome/browser/lacros/account_manager/profile_account_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

// static
ProfileAccountManagerFactory* ProfileAccountManagerFactory::GetInstance() {
  static base::NoDestructor<ProfileAccountManagerFactory> factory;
  return factory.get();
}

// static
ProfileAccountManager* ProfileAccountManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProfileAccountManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ProfileAccountManagerFactory::ProfileAccountManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ProfileAccountManager",
          BrowserContextDependencyManager::GetInstance()) {}

ProfileAccountManagerFactory::~ProfileAccountManagerFactory() = default;

KeyedService* ProfileAccountManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ProfileAccountManager(/*profile_path=*/context->GetPath());
}
