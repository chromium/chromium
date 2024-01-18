// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/string_value.pb.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/model_validator.h"
#endif  // BUILD_WITH_TFLITE_LIB

namespace {

// Delay at the startup before performing the model execution validation.
constexpr base::TimeDelta kModelExecutionValidationStartupDelay =
    base::Seconds(2);

}  // namespace

namespace optimization_guide {

ModelValidatorKeyedService::ModelValidatorKeyedService(Profile* profile)
    : profile_(profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ShouldStartModelValidator());
  auto* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!opt_guide_service) {
    return;
  }
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (switches::ShouldValidateModel()) {
    // Create the validator object which will get destroyed when the model
    // load is complete.
    new ModelValidatorHandler(
        opt_guide_service,
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
#endif  // BUILD_WITH_TFLITE_LIB
  if (switches::ShouldValidateModelExecution()) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    if (!identity_manager) {
      return;
    }
    if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      identity_manager_observation_.Observe(identity_manager);
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ModelValidatorKeyedService::StartModelExecutionValidation,
            weak_ptr_factory_.GetWeakPtr()),
        kModelExecutionValidationStartupDelay);
  }
}

ModelValidatorKeyedService::~ModelValidatorKeyedService() = default;

void ModelValidatorKeyedService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!switches::ShouldValidateModelExecution()) {
    return;
  }
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    return;
  }
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }
  identity_manager_observation_.Reset();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ModelValidatorKeyedService::StartModelExecutionValidation,
                     weak_ptr_factory_.GetWeakPtr()),
      kModelExecutionValidationStartupDelay);
}

void ModelValidatorKeyedService::StartModelExecutionValidation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!opt_guide_service) {
    return;
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string model_execution_input =
      command_line->GetSwitchValueASCII(switches::kModelExecutionValidate);
  if (model_execution_input.empty()) {
    return;
  }
  proto::StringValue request;
  request.set_value(model_execution_input);
  opt_guide_service->ExecuteModel(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST, request,
      base::BindOnce(&ModelValidatorKeyedService::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModelValidatorKeyedService::OnModelExecuteResponse(
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace optimization_guide
