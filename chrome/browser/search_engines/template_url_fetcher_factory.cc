// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_fetcher_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_fetcher.h"

// static
TemplateURLFetcher* TemplateURLFetcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TemplateURLFetcher*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TemplateURLFetcherFactory* TemplateURLFetcherFactory::GetInstance() {
  static base::NoDestructor<TemplateURLFetcherFactory> instance;
  return instance.get();
}

// static
void TemplateURLFetcherFactory::ShutdownForProfile(Profile* profile) {
  TemplateURLFetcherFactory* factory = GetInstance();
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
}

TemplateURLFetcherFactory::TemplateURLFetcherFactory()
    : ProfileKeyedServiceFactory(
          "TemplateURLFetcher",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

TemplateURLFetcherFactory::~TemplateURLFetcherFactory() = default;

std::unique_ptr<KeyedService>
TemplateURLFetcherFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<TemplateURLFetcher>(
      TemplateURLServiceFactory::GetForProfile(static_cast<Profile*>(profile)));
}
