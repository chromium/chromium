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
  return base::Singleton<ChromeColorsFactory>::get();
}

ChromeColorsFactory::ChromeColorsFactory()
    : ProfileKeyedServiceFactory("ChromeColorsService") {
  DependsOn(ThemeServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

ChromeColorsFactory::~ChromeColorsFactory() {}

KeyedService* ChromeColorsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeColorsService(Profile::FromBrowserContext(context));
}

}  // namespace chrome_colors
