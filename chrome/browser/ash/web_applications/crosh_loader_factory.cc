// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/crosh_loader_factory.h"

#include "chrome/browser/ash/web_applications/crosh_loader.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
CroshLoader* CroshLoaderFactory::GetForProfile(Profile* profile) {
  return static_cast<CroshLoader*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CroshLoaderFactory* CroshLoaderFactory::GetInstance() {
  static base::NoDestructor<CroshLoaderFactory> factory;
  return factory.get();
}

CroshLoaderFactory::CroshLoaderFactory()
    : BrowserContextKeyedServiceFactory(
          "CroshLoader",
          BrowserContextDependencyManager::GetInstance()) {}

CroshLoaderFactory::~CroshLoaderFactory() = default;

KeyedService* CroshLoaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new CroshLoader(Profile::FromBrowserContext(context));
}

content::BrowserContext* CroshLoaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool CroshLoaderFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CroshLoaderFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
