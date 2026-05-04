// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service_factory.h"

#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace dictation {

// static
DictationKeyedService* DictationKeyedServiceFactory::GetDictationKeyedService(
    content::BrowserContext* browser_context) {
  return static_cast<DictationKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
DictationKeyedServiceFactory* DictationKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<DictationKeyedServiceFactory> factory{
      base::PassKey<DictationKeyedServiceFactory>()};
  return factory.get();
}

DictationKeyedServiceFactory::DictationKeyedServiceFactory(
    base::PassKey<DictationKeyedServiceFactory>)
    : ProfileKeyedServiceFactory("DictationKeyedService",
                                 ProfileSelections::BuildForRegularProfile()) {}

DictationKeyedServiceFactory::~DictationKeyedServiceFactory() = default;

bool DictationKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
DictationKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kDictation)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<DictationKeyedService>(profile);
}

}  // namespace dictation
