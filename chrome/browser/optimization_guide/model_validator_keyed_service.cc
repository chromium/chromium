// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/string_value.pb.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/optimization_guide/core/model_validator.h"
#endif  // BUILD_WITH_TFLITE_LIB

namespace {

std::unique_ptr<optimization_guide::proto::ComposeRequest>
ParseComposeRequestFromFile(base::FilePath path) {
  std::string serialized_request;
  if (!base::ReadFileToString(path, &serialized_request)) {
    return nullptr;
  }
  auto request = std::make_unique<optimization_guide::proto::ComposeRequest>();
  if (!request->ParseFromString(serialized_request)) {
    return nullptr;
  }
  return request;
}

void WriteResponseToFile(base::FilePath path,
                         optimization_guide::proto::ComposeResponse response) {
  bool write_file_success = base::WriteFile(path, response.output());
  DCHECK(write_file_success);
}

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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelValidatorKeyedService::StartModelExecutionValidation,
            weak_ptr_factory_.GetWeakPtr()));
  }
  if (switches::GetOnDeviceValidationRequestOverride()) {
    base::FilePath ondevice_override_file =
        switches::GetOnDeviceValidationRequestOverride().value();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ParseComposeRequestFromFile, ondevice_override_file),
        base::BindOnce(
            &ModelValidatorKeyedService::StartOnDeviceModelExecutionValidation,
            weak_ptr_factory_.GetWeakPtr()));
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelValidatorKeyedService::StartModelExecutionValidation,
                     weak_ptr_factory_.GetWeakPtr()));
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

void ModelValidatorKeyedService::StartOnDeviceModelExecutionValidation(
    std::unique_ptr<optimization_guide::proto::ComposeRequest> request) {
  if (!request) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &ModelValidatorKeyedService::PerformOnDeviceModelExecutionValidation,
          weak_ptr_factory_.GetWeakPtr(), std::move(request)),
      features::GetOnDeviceModelExecutionValidationStartupDelay());
}

void ModelValidatorKeyedService::PerformOnDeviceModelExecutionValidation(
    std::unique_ptr<optimization_guide::proto::ComposeRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!opt_guide_service) {
    return;
  }
  on_device_validation_session_ = opt_guide_service->StartSession(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE,
      /*config_params=*/std::nullopt);
  on_device_validation_session_->ExecuteModel(
      *request, base::RepeatingCallback(base::BindRepeating(
                    &ModelValidatorKeyedService::OnDeviceModelExecuteResponse,
                    weak_ptr_factory_.GetWeakPtr())));
}

void ModelValidatorKeyedService::OnDeviceModelExecuteResponse(
    OptimizationGuideModelStreamingExecutionResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.response.has_value() || !result.response->is_complete) {
    return;
  }
  optimization_guide::proto::ComposeResponse compose_response;
  if (!compose_response.ParseFromString(result.response->response.value())) {
    return;
  }
  auto out_file = switches::GetOnDeviceValidationWriteToFile();
  if (!out_file) {
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteResponseToFile, *out_file, compose_response));
}

void ModelValidatorKeyedService::OnModelExecuteResponse(
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace optimization_guide
