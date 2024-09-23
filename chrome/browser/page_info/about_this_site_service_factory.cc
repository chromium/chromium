// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/about_this_site_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
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
    : ProfileKeyedServiceFactory(
          "AboutThisSiteServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

AboutThisSiteServiceFactory::~AboutThisSiteServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
AboutThisSiteServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  if (!page_info::IsAboutThisSiteFeatureEnabled(
          g_browser_process->GetApplicationLocale()))
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    return nullptr;
  }

  auto* template_service = TemplateURLServiceFactory::GetForProfile(profile);
  // TemplateURLService may be null during testing.
  if (!template_service) {
    return nullptr;
  }

  return std::make_unique<page_info::AboutThisSiteService>(
      optimization_guide, profile->IsOffTheRecord(), profile->GetPrefs(),
      template_service);
}

bool AboutThisSiteServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // This service needs to be created at startup in order to register its OptimizationType with
  // OptimizationGuideDecider.
  return true;
}
