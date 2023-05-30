// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/profiles/profile.h"

namespace predictors {

// static
PredictorDatabase* PredictorDatabaseFactory::GetForProfile(Profile* profile) {
  return static_cast<PredictorDatabase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PredictorDatabaseFactory* PredictorDatabaseFactory::GetInstance() {
  static base::NoDestructor<PredictorDatabaseFactory> instance;
  return instance.get();
}

PredictorDatabaseFactory::PredictorDatabaseFactory()
    : ProfileKeyedServiceFactory(
          "PredictorDatabase",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

PredictorDatabaseFactory::~PredictorDatabaseFactory() = default;

KeyedService* PredictorDatabaseFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  return new PredictorDatabase(static_cast<Profile*>(profile),
                               std::move(db_task_runner));
}

}  // namespace predictors
