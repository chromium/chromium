// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_keyed_service_factory.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"

// static
LCPCriticalPathPredictorKeyedService*
LCPCriticalPathPredictorKeyedServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LCPCriticalPathPredictorKeyedService*>(
      GetInstance().GetServiceForBrowserContext(profile, true));
}

// static
LCPCriticalPathPredictorKeyedServiceFactory&
LCPCriticalPathPredictorKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<LCPCriticalPathPredictorKeyedServiceFactory>
      instance;
  return *instance;
}

LCPCriticalPathPredictorKeyedServiceFactory::
    LCPCriticalPathPredictorKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "LCPCriticalPathPredictorKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

LCPCriticalPathPredictorKeyedServiceFactory::
    ~LCPCriticalPathPredictorKeyedServiceFactory() = default;

std::unique_ptr<KeyedService> LCPCriticalPathPredictorKeyedServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  return std::make_unique<LCPCriticalPathPredictorKeyedService>(
      Profile::FromBrowserContext(context), std::move(db_task_runner));
}
