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
  return base::Singleton<TemplateURLFetcherFactory>::get();
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
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

TemplateURLFetcherFactory::~TemplateURLFetcherFactory() {
}

KeyedService* TemplateURLFetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new TemplateURLFetcher(
      TemplateURLServiceFactory::GetForProfile(static_cast<Profile*>(profile)));
}
