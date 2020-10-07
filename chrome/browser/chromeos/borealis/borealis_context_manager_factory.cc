// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_factory.h"

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace borealis {

// static
BorealisContextManager* BorealisContextManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BorealisContextManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BorealisContextManagerFactory* BorealisContextManagerFactory::GetInstance() {
  static base::NoDestructor<BorealisContextManagerFactory> factory;
  return factory.get();
}

BorealisContextManagerFactory::BorealisContextManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "BorealisContextManager",
          BrowserContextDependencyManager::GetInstance()) {}

BorealisContextManagerFactory::~BorealisContextManagerFactory() = default;

KeyedService* BorealisContextManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return new BorealisContextManagerImpl(profile);
  return nullptr;
}

content::BrowserContext* BorealisContextManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace borealis
