// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_factory.h"

#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"

namespace chrome_colors {

// static
ChromeColorsService* ChromeColorsFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeColorsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeColorsFactory* ChromeColorsFactory::GetInstance() {
  static base::NoDestructor<ChromeColorsFactory> instance;
  return instance.get();
}

ChromeColorsFactory::ChromeColorsFactory()
    : ProfileKeyedServiceFactory(
          "ChromeColorsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

ChromeColorsFactory::~ChromeColorsFactory() = default;

std::unique_ptr<KeyedService>
ChromeColorsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeColorsService>(
      Profile::FromBrowserContext(context));
}

}  // namespace chrome_colors
