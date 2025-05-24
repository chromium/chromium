// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class SearchPreloadService;
class Profile;

// LazyInstance that owns all SearchPreloadServices and associates them
// with Profiles.
class SearchPreloadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the SearchPreloadService for the profile.
  //
  // Returns null if the feature `kDsePreload2` is disabled.
  static SearchPreloadService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all SearchPreloadService(s).
  static SearchPreloadServiceFactory* GetInstance();

  SearchPreloadServiceFactory(const SearchPreloadServiceFactory&) = delete;
  SearchPreloadServiceFactory& operator=(const SearchPreloadServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<SearchPreloadServiceFactory>;

  SearchPreloadServiceFactory();
  ~SearchPreloadServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_SERVICE_FACTORY_H_
