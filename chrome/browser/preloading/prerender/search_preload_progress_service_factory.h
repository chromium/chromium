// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PRELOAD_PROGRESS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PRELOAD_PROGRESS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class SearchPreloadProgressService;

// Factory for SearchPreloadProgressService.
class SearchPreloadProgressServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SearchPreloadProgressService* GetForProfile(Profile* profile);

  static SearchPreloadProgressServiceFactory* GetInstance();

  SearchPreloadProgressServiceFactory(
      const SearchPreloadProgressServiceFactory&) = delete;
  SearchPreloadProgressServiceFactory& operator=(
      const SearchPreloadProgressServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SearchPreloadProgressServiceFactory>;

  SearchPreloadProgressServiceFactory();
  ~SearchPreloadProgressServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PRELOAD_PROGRESS_SERVICE_FACTORY_H_
