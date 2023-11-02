// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace predictors {

// static
LoadingPredictor* LoadingPredictorFactory::GetForProfile(Profile* profile) {
  return static_cast<LoadingPredictor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LoadingPredictorFactory* LoadingPredictorFactory::GetInstance() {
  return base::Singleton<LoadingPredictorFactory>::get();
}

LoadingPredictorFactory::LoadingPredictorFactory()
    : ProfileKeyedServiceFactory("LoadingPredictor") {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PredictorDatabaseFactory::GetInstance());
}

LoadingPredictorFactory::~LoadingPredictorFactory() {}

KeyedService* LoadingPredictorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!IsLoadingPredictorEnabled(profile))
    return nullptr;

  return new LoadingPredictor(LoadingPredictorConfig(), profile);
}

}  // namespace predictors
