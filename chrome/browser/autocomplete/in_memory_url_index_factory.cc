// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "content/public/common/url_constants.h"

// static
InMemoryURLIndex* InMemoryURLIndexFactory::GetForProfile(Profile* profile) {
  return static_cast<InMemoryURLIndex*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InMemoryURLIndexFactory* InMemoryURLIndexFactory::GetInstance() {
  static base::NoDestructor<InMemoryURLIndexFactory> instance;
  return instance.get();
}

InMemoryURLIndexFactory::InMemoryURLIndexFactory()
    : ProfileKeyedServiceFactory(
          "InMemoryURLIndex",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

InMemoryURLIndexFactory::~InMemoryURLIndexFactory() = default;

std::unique_ptr<KeyedService>
InMemoryURLIndexFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Do not force creation of the HistoryService if saving history is disabled.
  Profile* profile = Profile::FromBrowserContext(context);
  SchemeSet chrome_schemes_to_whitelist;
  chrome_schemes_to_whitelist.insert(content::kChromeUIScheme);
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index =
      std::make_unique<InMemoryURLIndex>(
          BookmarkModelFactory::GetForBrowserContext(profile),
          HistoryServiceFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS),
          TemplateURLServiceFactory::GetForProfile(profile), profile->GetPath(),
          chrome_schemes_to_whitelist);
  in_memory_url_index->Init();
  return in_memory_url_index;
}

bool InMemoryURLIndexFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
