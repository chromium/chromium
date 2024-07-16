// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_factory.h"

#include "chrome/browser/accessibility/page_colors.h"
#include "chrome/browser/profiles/profile.h"

// static
PageColors* PageColorsFactory::GetForProfile(Profile* profile) {
  return static_cast<PageColors*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PageColorsFactory* PageColorsFactory::GetInstance() {
  static base::NoDestructor<PageColorsFactory> instance;
  return instance.get();
}

PageColorsFactory::PageColorsFactory()
    : ProfileKeyedServiceFactory(
          "PageColors",
          ProfileSelections::Builder()
              // The incognito profile shares the PageColors with it's original
              // profile.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

PageColorsFactory::~PageColorsFactory() = default;

bool PageColorsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
PageColorsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto page_colors = std::make_unique<PageColors>(
      Profile::FromBrowserContext(context)->GetPrefs());
  page_colors->Init();
  return page_colors;
}
