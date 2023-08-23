// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace predictors {

class AutocompleteActionPredictor;

// Singleton that owns all AutocompleteActionPredictors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AutocompleteActionPredictor.
class AutocompleteActionPredictorFactory : public ProfileKeyedServiceFactory {
 public:
  static AutocompleteActionPredictor* GetForProfile(Profile* profile);

  static AutocompleteActionPredictorFactory* GetInstance();

  AutocompleteActionPredictorFactory(
      const AutocompleteActionPredictorFactory&) = delete;
  AutocompleteActionPredictorFactory& operator=(
      const AutocompleteActionPredictorFactory&) = delete;

 private:
  friend base::NoDestructor<AutocompleteActionPredictorFactory>;

  AutocompleteActionPredictorFactory();
  ~AutocompleteActionPredictorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_
