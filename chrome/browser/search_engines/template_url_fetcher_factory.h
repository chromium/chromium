// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class TemplateURLFetcher;

// Singleton that owns all TemplateURLFetcher and associates them with
// Profiles.
class TemplateURLFetcherFactory : public ProfileKeyedServiceFactory {
 public:
  static TemplateURLFetcher* GetForProfile(Profile* profile);

  static TemplateURLFetcherFactory* GetInstance();

  TemplateURLFetcherFactory(const TemplateURLFetcherFactory&) = delete;
  TemplateURLFetcherFactory& operator=(const TemplateURLFetcherFactory&) =
      delete;

  // In some tests, the template url fetcher needs to be shutdown to
  // remove any dangling url requests before the io_thread is shutdown
  // to prevent leaks.
  static void ShutdownForProfile(Profile* profile);

 private:
  friend base::NoDestructor<TemplateURLFetcherFactory>;

  TemplateURLFetcherFactory();
  ~TemplateURLFetcherFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_FACTORY_H_
