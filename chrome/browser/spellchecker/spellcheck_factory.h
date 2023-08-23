// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SpellcheckService;

// Entry into the SpellCheck system.
//
// Internally, this owns all SpellcheckService objects.
class SpellcheckServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the spell check host. This will create the SpellcheckService
  // if it does not already exist. This can return NULL.
  static SpellcheckService* GetForContext(content::BrowserContext* context);

  static SpellcheckServiceFactory* GetInstance();

  SpellcheckServiceFactory(const SpellcheckServiceFactory&) = delete;
  SpellcheckServiceFactory& operator=(const SpellcheckServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SpellcheckServiceFactory>;

  SpellcheckServiceFactory();
  ~SpellcheckServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;

  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT);
#if BUILDFLAG(IS_WIN)
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceWindowsHybridBrowserTest,
                           WindowsHybridSpellcheck);
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceWindowsHybridBrowserTestDelayInit,
                           WindowsHybridSpellcheckDelayInit);
#endif  // BUILDFLAG(IS_WIN)
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
