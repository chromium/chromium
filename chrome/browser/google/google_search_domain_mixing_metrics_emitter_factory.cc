// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/history/history_service_factory.h"

BASE_FEATURE(kEmitGoogleSearchDomainMixingMetrics,
             "EmitGoogleSearchDomainMixingMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
GoogleSearchDomainMixingMetricsEmitterFactory*
GoogleSearchDomainMixingMetricsEmitterFactory::GetInstance() {
  return base::Singleton<GoogleSearchDomainMixingMetricsEmitterFactory>::get();
}

// static
GoogleSearchDomainMixingMetricsEmitter*
GoogleSearchDomainMixingMetricsEmitterFactory::GetForProfile(Profile* profile) {
  return static_cast<GoogleSearchDomainMixingMetricsEmitter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

GoogleSearchDomainMixingMetricsEmitterFactory::
    GoogleSearchDomainMixingMetricsEmitterFactory()
    : ProfileKeyedServiceFactory("GoogleSearchDomainMixingMetricsEmitter") {
  DependsOn(HistoryServiceFactory::GetInstance());
}

void GoogleSearchDomainMixingMetricsEmitterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  GoogleSearchDomainMixingMetricsEmitter::RegisterProfilePrefs(registry);
}

KeyedService*
GoogleSearchDomainMixingMetricsEmitterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* history_service = HistoryServiceFactory::GetInstance()->GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service == nullptr)
    return nullptr;
  auto emitter = std::make_unique<GoogleSearchDomainMixingMetricsEmitter>(
      profile->GetPrefs(), history_service);
  emitter->Start();
  return emitter.release();
}

bool GoogleSearchDomainMixingMetricsEmitterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(kEmitGoogleSearchDomainMixingMetrics);
}
