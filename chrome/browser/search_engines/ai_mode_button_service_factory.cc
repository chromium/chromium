// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/ai_mode_button_service.h"

// static
AiModeButtonService* AiModeButtonServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AiModeButtonService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AiModeButtonServiceFactory* AiModeButtonServiceFactory::GetInstance() {
  static base::NoDestructor<AiModeButtonServiceFactory> instance;
  return instance.get();
}

AiModeButtonServiceFactory::AiModeButtonServiceFactory()
    : ProfileKeyedServiceFactory(
          "AiModeButtonService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

AiModeButtonServiceFactory::~AiModeButtonServiceFactory() = default;

std::unique_ptr<KeyedService>
AiModeButtonServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return nullptr;
  }
  return std::make_unique<AiModeButtonService>(template_url_service);
}
