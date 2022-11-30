// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_DOMAIN_MIXING_METRICS_EMITTER_FACTORY_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_DOMAIN_MIXING_METRICS_EMITTER_FACTORY_H_

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

// Flag to enable computing domain mixing metrics based on the Google search
// activity of the user.
// For more details, see http://goto.google.com/chrome-no-searchdomaincheck.
BASE_DECLARE_FEATURE(kEmitGoogleSearchDomainMixingMetrics);

// Singleton that owns all GoogleSearchDomainMixingMetricsEmitters and
// associates them with Profiles.
class GoogleSearchDomainMixingMetricsEmitterFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the singleton instance of the factory.
  static GoogleSearchDomainMixingMetricsEmitterFactory* GetInstance();

  // Returns the GoogleSearchDomainMixingMetricsEmitter for |profile|, creating
  // one if needed. May return nullptr if there is no emitter should be created
  // for the profile, e.g. in incognito mode.
  static GoogleSearchDomainMixingMetricsEmitter* GetForProfile(
      Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<
      GoogleSearchDomainMixingMetricsEmitterFactory>;
  friend class GoogleSearchDomainMixingMetricsEmitterFactoryTest;

  GoogleSearchDomainMixingMetricsEmitterFactory();

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_DOMAIN_MIXING_METRICS_EMITTER_FACTORY_H_
