// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace predictors {

// static
PredictorDatabase* PredictorDatabaseFactory::GetForProfile(Profile* profile) {
  return static_cast<PredictorDatabase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PredictorDatabaseFactory* PredictorDatabaseFactory::GetInstance() {
  return base::Singleton<PredictorDatabaseFactory>::get();
}

PredictorDatabaseFactory::PredictorDatabaseFactory()
    : BrowserContextKeyedServiceFactory(
        "PredictorDatabase", BrowserContextDependencyManager::GetInstance()) {
}

PredictorDatabaseFactory::~PredictorDatabaseFactory() {
}

KeyedService* PredictorDatabaseFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  return new PredictorDatabase(static_cast<Profile*>(profile),
                               std::move(db_task_runner));
}

}  // namespace predictors
