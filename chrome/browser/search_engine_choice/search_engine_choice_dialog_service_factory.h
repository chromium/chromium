// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_SERVICE_FACTORY_H_

#include "base/auto_reset.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace search_engines {
enum class SearchEngineChoiceScreenConditions;
}

class SearchEngineChoiceDialogService;
class KeyedService;

class SearchEngineChoiceDialogServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  SearchEngineChoiceDialogServiceFactory(
      const SearchEngineChoiceDialogServiceFactory&) = delete;
  SearchEngineChoiceDialogServiceFactory& operator=(
      const SearchEngineChoiceDialogServiceFactory&) = delete;

  static SearchEngineChoiceDialogService* GetForProfile(Profile* profile);

  static SearchEngineChoiceDialogServiceFactory* GetInstance();

  // Checks that the profile is the chosen one to display the choice dialog.
  // If none is chosen yet and `try_claim` is `true`, then `profile` will be
  // marked as the chosen one.
  // TODO(b/309936758): Deprecated, currently always returns `true`.
  static bool IsSelectedChoiceProfile(Profile& profile, bool try_claim);

  // Overrides the check for branded build. This allows bots that run on
  // non-branded builds to test the code.
  static base::AutoReset<bool> ScopedChromeBuildOverrideForTesting(
      bool force_chrome_build);

  // Checks static conditions for the profile and logs them to histograms.
  // Exposes an internal helper and should only be used for testing purposes.
  static bool IsProfileEligibleForChoiceScreenForTesting(Profile& profile);

 private:
  friend class base::NoDestructor<SearchEngineChoiceDialogServiceFactory>;

  SearchEngineChoiceDialogServiceFactory();
  ~SearchEngineChoiceDialogServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_SERVICE_FACTORY_H_
