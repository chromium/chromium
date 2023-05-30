// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace predictors {

// static
AutocompleteActionPredictor* AutocompleteActionPredictorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutocompleteActionPredictor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutocompleteActionPredictorFactory*
    AutocompleteActionPredictorFactory::GetInstance() {
  static base::NoDestructor<AutocompleteActionPredictorFactory> instance;
  return instance.get();
}

AutocompleteActionPredictorFactory::AutocompleteActionPredictorFactory()
    : ProfileKeyedServiceFactory(
          "AutocompleteActionPredictor",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PredictorDatabaseFactory::GetInstance());
}

AutocompleteActionPredictorFactory::~AutocompleteActionPredictorFactory() =
    default;

KeyedService* AutocompleteActionPredictorFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AutocompleteActionPredictor(static_cast<Profile*>(profile));
}

}  // namespace predictors
