// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/language_packs/language_pack_font_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

class KeyedService;

namespace ash::language_packs {

LanguagePackFontServiceFactory* LanguagePackFontServiceFactory::GetInstance() {
  static base::NoDestructor<LanguagePackFontServiceFactory> instance;
  return instance.get();
}

LanguagePackFontServiceFactory::LanguagePackFontServiceFactory()
    : ProfileKeyedServiceFactory(
          "FontManagerFactory",
          ProfileSelections::Builder()
              // OTR renderers in Ash will inherit fontconfig from the original.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // No DLC fonts if there is no user.
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

LanguagePackFontServiceFactory::~LanguagePackFontServiceFactory() = default;

std::unique_ptr<KeyedService>
LanguagePackFontServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kLanguagePacksFonts)) {
    return nullptr;
  }
  return std::make_unique<LanguagePackFontService>(
      Profile::FromBrowserContext(context)->GetPrefs());
}

bool LanguagePackFontServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash::language_packs
