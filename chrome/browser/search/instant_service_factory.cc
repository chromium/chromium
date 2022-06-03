// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search/search.h"

// static
InstantService* InstantServiceFactory::GetForProfile(Profile* profile) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  return static_cast<InstantService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstantServiceFactory* InstantServiceFactory::GetInstance() {
  return base::Singleton<InstantServiceFactory>::get();
}

InstantServiceFactory::InstantServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "InstantService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

InstantServiceFactory::~InstantServiceFactory() = default;

content::BrowserContext* InstantServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* InstantServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(search::IsInstantExtendedAPIEnabled());
  return new InstantService(Profile::FromBrowserContext(context));
}
