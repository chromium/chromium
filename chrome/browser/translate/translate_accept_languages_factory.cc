// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_accept_languages_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_accept_languages.h"

// static
TranslateAcceptLanguagesFactory*
TranslateAcceptLanguagesFactory::GetInstance() {
  return base::Singleton<TranslateAcceptLanguagesFactory>::get();
}

// static
translate::TranslateAcceptLanguages*
TranslateAcceptLanguagesFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<translate::TranslateAcceptLanguages*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

TranslateAcceptLanguagesFactory::TranslateAcceptLanguagesFactory()
    : BrowserContextKeyedServiceFactory(
          "TranslateAcceptLanguages",
          BrowserContextDependencyManager::GetInstance()) {}

TranslateAcceptLanguagesFactory::~TranslateAcceptLanguagesFactory() {}

KeyedService* TranslateAcceptLanguagesFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new translate::TranslateAcceptLanguages(
      profile->GetPrefs(), language::prefs::kAcceptLanguages);
}

content::BrowserContext*
TranslateAcceptLanguagesFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
