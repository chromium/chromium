// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/iban_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/iban_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
IBANManager* IBANManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<IBANManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
IBANManagerFactory* IBANManagerFactory::GetInstance() {
  return base::Singleton<IBANManagerFactory>::get();
}

IBANManagerFactory::IBANManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "IBANManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
}

IBANManagerFactory::~IBANManagerFactory() = default;

KeyedService* IBANManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  IBANManager* service =
      new IBANManager(PersonalDataManagerFactory::GetForBrowserContext(context),
                      profile->IsOffTheRecord());
  return service;
}

content::BrowserContext* IBANManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace autofill
