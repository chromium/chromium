// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/grit/locale_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/l10n/l10n_util.h"

// static
SpellcheckService* SpellcheckServiceFactory::GetForContext(
    content::BrowserContext* context) {
  return static_cast<SpellcheckService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SpellcheckServiceFactory* SpellcheckServiceFactory::GetInstance() {
  return base::Singleton<SpellcheckServiceFactory>::get();
}

SpellcheckServiceFactory::SpellcheckServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "SpellcheckService",
        BrowserContextDependencyManager::GetInstance()) {
  // TODO(erg): Uncomment these as they are initialized.
  // DependsOn(RequestContextFactory::GetInstance());
}

SpellcheckServiceFactory::~SpellcheckServiceFactory() {}

KeyedService* SpellcheckServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Many variables are initialized from the |context| in the SpellcheckService.
  SpellcheckService* spellcheck = new SpellcheckService(context);

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  DCHECK(prefs);

  // Instantiates Metrics object for spellchecking for use.
  spellcheck->StartRecordingMetrics(
      prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable));

  return spellcheck;
}

void SpellcheckServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterListPref(spellcheck::prefs::kSpellCheckDictionaries);
  user_prefs->RegisterListPref(
      spellcheck::prefs::kSpellCheckForcedDictionaries);
  user_prefs->RegisterListPref(
      spellcheck::prefs::kSpellCheckBlacklistedDictionaries);
  // Continue registering kSpellCheckDictionary for preference migration.
  // TODO(estade): remove: crbug.com/751275
  user_prefs->RegisterStringPref(
      spellcheck::prefs::kSpellCheckDictionary,
      l10n_util::GetStringUTF8(IDS_SPELLCHECK_DICTIONARY));
  user_prefs->RegisterBooleanPref(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);
#if defined(OS_ANDROID)
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  user_prefs->RegisterBooleanPref(spellcheck::prefs::kSpellCheckEnable, true,
                                  flags);
}

content::BrowserContext* SpellcheckServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SpellcheckServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
