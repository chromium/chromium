// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/accept_languages_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"

// static
AcceptLanguagesServiceFactory* AcceptLanguagesServiceFactory::GetInstance() {
  return base::Singleton<AcceptLanguagesServiceFactory>::get();
}

// static
language::AcceptLanguagesService*
AcceptLanguagesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<language::AcceptLanguagesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AcceptLanguagesServiceFactory::AcceptLanguagesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AcceptLanguagesService",
          BrowserContextDependencyManager::GetInstance()) {}

AcceptLanguagesServiceFactory::~AcceptLanguagesServiceFactory() {}

KeyedService* AcceptLanguagesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new language::AcceptLanguagesService(
      profile->GetPrefs(), language::prefs::kAcceptLanguages);
}

content::BrowserContext* AcceptLanguagesServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
