// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_integrity/search_integrity.h"

namespace search_integrity {

// static
SearchIntegrity* SearchIntegrityFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchIntegrity*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SearchIntegrityFactory* SearchIntegrityFactory::GetInstance() {
  static base::NoDestructor<SearchIntegrityFactory> instance;
  return instance.get();
}

SearchIntegrityFactory::SearchIntegrityFactory()
    : ProfileKeyedServiceFactory(
          "SearchIntegrity",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchIntegrityFactory::~SearchIntegrityFactory() = default;

std::unique_ptr<KeyedService>
SearchIntegrityFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SearchIntegrity>(
      TemplateURLServiceFactory::GetForProfile(profile), profile->GetPath());
}

}  // namespace search_integrity
