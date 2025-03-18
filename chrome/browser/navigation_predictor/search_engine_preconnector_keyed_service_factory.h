// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_KEYED_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class SearchEnginePreconnector;
class Profile;

// LazyInstance that owns all SearchEnginePreconnector and associates
// them with Profiles.
class SearchEnginePreconnectorKeyedServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets the SearchEnginePreconnector instance for |profile|.
  static SearchEnginePreconnector* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all SearchEnginePreconnector.
  static SearchEnginePreconnectorKeyedServiceFactory* GetInstance();

  SearchEnginePreconnectorKeyedServiceFactory(
      const SearchEnginePreconnectorKeyedServiceFactory&) = delete;
  SearchEnginePreconnectorKeyedServiceFactory& operator=(
      const SearchEnginePreconnectorKeyedServiceFactory&) = delete;

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::LazyInstanceTraitsBase<
      SearchEnginePreconnectorKeyedServiceFactory>;

  SearchEnginePreconnectorKeyedServiceFactory();
  ~SearchEnginePreconnectorKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_KEYED_SERVICE_FACTORY_H_
