// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_controller_factory.h"

#include "chrome/browser/accessibility/page_colors_controller.h"
#include "chrome/browser/profiles/profile.h"

// static
PageColorsController* PageColorsControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PageColorsController*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PageColorsControllerFactory* PageColorsControllerFactory::GetInstance() {
  static base::NoDestructor<PageColorsControllerFactory> instance;
  return instance.get();
}

PageColorsControllerFactory::PageColorsControllerFactory()
    : ProfileKeyedServiceFactory(
          "PageColorsController",
          ProfileSelections::Builder()
              // The incognito profile shares the PageColorsController with its
              // original profile.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

PageColorsControllerFactory::~PageColorsControllerFactory() = default;

bool PageColorsControllerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
PageColorsControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PageColorsController>(
      Profile::FromBrowserContext(context)->GetPrefs());
}
