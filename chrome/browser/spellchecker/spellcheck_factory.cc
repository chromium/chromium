// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_factory.h"

#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/grit/locale_settings.h"
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
  static base::NoDestructor<SpellcheckServiceFactory> instance;
  return instance.get();
}

SpellcheckServiceFactory::SpellcheckServiceFactory()
    : ProfileKeyedServiceFactory(
          "SpellcheckService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  // TODO(erg): Uncomment these as they are initialized.
  // DependsOn(RequestContextFactory::GetInstance());
}

SpellcheckServiceFactory::~SpellcheckServiceFactory() = default;

std::unique_ptr<KeyedService>
SpellcheckServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Many variables are initialized from the |context| in the SpellcheckService.
  return std::make_unique<SpellcheckService>(context);
}

void SpellcheckServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterListPref(spellcheck::prefs::kSpellCheckDictionaries);
  user_prefs->RegisterListPref(
      spellcheck::prefs::kSpellCheckForcedDictionaries);
  user_prefs->RegisterListPref(
      spellcheck::prefs::kSpellCheckBlocklistedDictionaries);
  // Continue registering kSpellCheckDictionary for preference migration.
  // TODO(estade): remove: crbug.com/751275
  user_prefs->RegisterStringPref(
      spellcheck::prefs::kSpellCheckDictionary,
      l10n_util::GetStringUTF8(IDS_SPELLCHECK_DICTIONARY));
  user_prefs->RegisterBooleanPref(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);
#if BUILDFLAG(IS_ANDROID)
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  user_prefs->RegisterBooleanPref(spellcheck::prefs::kSpellCheckEnable, true,
                                  flags);
}

bool SpellcheckServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
