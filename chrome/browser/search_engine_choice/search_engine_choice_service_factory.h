// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_

#include "base/auto_reset.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {
class PolicyService;
}

class SearchEngineChoiceService;
class KeyedService;

class SearchEngineChoiceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  SearchEngineChoiceServiceFactory(const SearchEngineChoiceServiceFactory&) =
      delete;
  SearchEngineChoiceServiceFactory& operator=(
      const SearchEngineChoiceServiceFactory&) = delete;

  static SearchEngineChoiceService* GetForProfile(Profile* profile);

  static SearchEngineChoiceServiceFactory* GetInstance();

  // Overrides the check for branded build. This allows bots that run on
  // non-branded builds to test the code.
  static base::AutoReset<bool> ScopedChromeBuildOverrideForTesting(
      bool force_chrome_build);

 private:
  friend class base::NoDestructor<SearchEngineChoiceServiceFactory>;

  SearchEngineChoiceServiceFactory();
  ~SearchEngineChoiceServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Returns whether the profile is eligible for the Search Engine Choice dialog
  // based on device policies and profile attributes.
  bool IsProfileEligibleForChoiceScreen(
      const policy::PolicyService& policy_service,
      Profile& profile) const;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H
