// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/about_this_site_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_info/chrome_about_this_site_service_client.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"

// static
page_info::AboutThisSiteService* AboutThisSiteServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<page_info::AboutThisSiteService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
// static
AboutThisSiteServiceFactory* AboutThisSiteServiceFactory::GetInstance() {
  static base::NoDestructor<AboutThisSiteServiceFactory> factory;
  return factory.get();
}

AboutThisSiteServiceFactory::AboutThisSiteServiceFactory()
    : ProfileKeyedServiceFactory("AboutThisSiteServiceFactory") {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

AboutThisSiteServiceFactory::~AboutThisSiteServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* AboutThisSiteServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  if (!page_info::IsAboutThisSiteFeatureEnabled(
          g_browser_process->GetApplicationLocale()))
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  return new page_info::AboutThisSiteService(
      std::make_unique<ChromeAboutThisSiteServiceClient>(
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
          profile->IsOffTheRecord(), profile->GetPrefs()),
      TemplateURLServiceFactory::GetForProfile(profile),
      page_info::IsDescriptionPlaceholderFeatureEnabled());
}

bool AboutThisSiteServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // This service needs to be created at startup in order to register its OptimizationType with
  // OptimizationGuideDecider.
  return true;
}
