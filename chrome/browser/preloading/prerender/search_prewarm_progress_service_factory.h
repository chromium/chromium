// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class SearchPrewarmProgressService;

// Factory for SearchPrewarmProgressService.
class SearchPrewarmProgressServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SearchPrewarmProgressService* GetForProfile(Profile* profile);

  static SearchPrewarmProgressServiceFactory* GetInstance();

  SearchPrewarmProgressServiceFactory(
      const SearchPrewarmProgressServiceFactory&) = delete;
  SearchPrewarmProgressServiceFactory& operator=(
      const SearchPrewarmProgressServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SearchPrewarmProgressServiceFactory>;

  SearchPrewarmProgressServiceFactory();
  ~SearchPrewarmProgressServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_FACTORY_H_
