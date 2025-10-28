// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/contextual_search/contextual_search_service.h"

class Profile;

class ContextualSearchServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static contextual_search::ContextualSearchService* GetForProfile(
      Profile* profile);
  static ContextualSearchServiceFactory* GetInstance();

  ContextualSearchServiceFactory(const ContextualSearchServiceFactory&) =
      delete;
  ContextualSearchServiceFactory& operator=(
      const ContextualSearchServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ContextualSearchServiceFactory>;

  ContextualSearchServiceFactory();
  ~ContextualSearchServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_
