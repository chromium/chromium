// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/provider_state_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/omnibox/browser/provider_state_service.h"
#include "content/public/browser/browser_context.h"

// static
ProviderStateService* ProviderStateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProviderStateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProviderStateServiceFactory* ProviderStateServiceFactory::GetInstance() {
  static base::NoDestructor<ProviderStateServiceFactory> instance;
  return instance.get();
}

ProviderStateServiceFactory::ProviderStateServiceFactory()
    : ProfileKeyedServiceFactory("ProviderStateService",
                                 ProfileSelections::Builder()
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {}

ProviderStateServiceFactory::~ProviderStateServiceFactory() = default;

std::unique_ptr<KeyedService>
ProviderStateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ProviderStateService>();
}
