// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class SearchPrefetchService;
class Profile;

// LazyInstance that owns all SearchPrefetchServices and associates them
// with Profiles.
class SearchPrefetchServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the SearchPrefetchService for the profile.
  //
  // Returns null if the features if not enabled or incognito.
  static SearchPrefetchService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all SearchPrefetchService(s).
  static SearchPrefetchServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SearchPrefetchServiceFactory>;

  SearchPrefetchServiceFactory();
  ~SearchPrefetchServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SearchPrefetchServiceFactory);
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_FACTORY_H_
