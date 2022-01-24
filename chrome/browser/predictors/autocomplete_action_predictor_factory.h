// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace predictors {

class AutocompleteActionPredictor;

// Singleton that owns all AutocompleteActionPredictors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AutocompleteActionPredictor.
class AutocompleteActionPredictorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AutocompleteActionPredictor* GetForProfile(Profile* profile);

  static AutocompleteActionPredictorFactory* GetInstance();

  AutocompleteActionPredictorFactory(
      const AutocompleteActionPredictorFactory&) = delete;
  AutocompleteActionPredictorFactory& operator=(
      const AutocompleteActionPredictorFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      AutocompleteActionPredictorFactory>;

  AutocompleteActionPredictorFactory();
  ~AutocompleteActionPredictorFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_
