// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/factories/password_counter_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/password_manager/core/browser/password_counter.h"

PasswordCounterFactory::PasswordCounterFactory()
    : ProfileKeyedServiceFactory(
          "PasswordCounterFactory",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
}

PasswordCounterFactory::~PasswordCounterFactory() = default;

PasswordCounterFactory* PasswordCounterFactory::GetInstance() {
  static base::NoDestructor<PasswordCounterFactory> instance;
  return instance.get();
}

password_manager::PasswordCounter* PasswordCounterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<password_manager::PasswordCounter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

bool PasswordCounterFactory::ServiceIsCreatedWithBrowserContext() const {
  // This service is a cache of PasswordStore state, therefore it must be
  // initialized before the value is ever looked up.
  return true;
}

std::unique_ptr<KeyedService>
PasswordCounterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<password_manager::PasswordCounter>(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get());
}
