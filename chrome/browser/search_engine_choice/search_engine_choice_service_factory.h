// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class KeyedService;

namespace search_engines {

class SearchEngineChoiceService;

class SearchEngineChoiceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  SearchEngineChoiceServiceFactory(const SearchEngineChoiceServiceFactory&) =
      delete;
  SearchEngineChoiceServiceFactory& operator=(
      const SearchEngineChoiceServiceFactory&) = delete;

  static SearchEngineChoiceService* GetForProfile(Profile* profile);

  static SearchEngineChoiceServiceFactory* GetInstance();

  // Returns the default factory used to build SearchEngineChoiceService. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SearchEngineChoiceServiceFactory>;

  SearchEngineChoiceServiceFactory();
  ~SearchEngineChoiceServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace search_engines

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
