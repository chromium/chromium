// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/model_validator.h"
#endif  // BUILD_WITH_TFLITE_LIB

namespace optimization_guide {

ModelValidatorKeyedService::ModelValidatorKeyedService(Profile* profile) {
  DCHECK(switches::ShouldValidateModel());
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  auto* model_provider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!model_provider)
    return;

  // Create the validator object which will get destroyed when the model
  // load is complete.
  new ModelValidatorHandler(
      model_provider, base::ThreadPool::CreateSequencedTaskRunner(
                          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
#endif  // BUILD_WITH_TFLITE_LIB
}

ModelValidatorKeyedService::~ModelValidatorKeyedService() = default;

}  // namespace optimization_guide
