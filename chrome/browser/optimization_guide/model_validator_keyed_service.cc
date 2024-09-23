// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_validation.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/optimization_guide/core/model_validator.h"
#endif  // BUILD_WITH_TFLITE_LIB

namespace {

std::unique_ptr<optimization_guide::proto::ModelValidationInput>
ParseRequestFromFile(base::FilePath path) {
  std::string serialized_request;
  if (!base::ReadFileToString(path, &serialized_request)) {
    return nullptr;
  }
  auto request =
      std::make_unique<optimization_guide::proto::ModelValidationInput>();
  if (!request->ParseFromString(serialized_request)) {
    return nullptr;
  }
  return request;
}

void WriteResponseToFile(
    base::FilePath path,
    optimization_guide::proto::ModelValidationOutput validation_output) {
  std::string serialized_output;
  if (!validation_output.SerializeToString(&serialized_output)) {
    return;
  }
  bool write_file_success = base::WriteFile(path, serialized_output);
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
        base::BindOnce(&ParseRequestFromFile, ondevice_override_file),
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
      ModelBasedCapabilityKey::kTest, request,
      base::BindOnce(&ModelValidatorKeyedService::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModelValidatorKeyedService::StartOnDeviceModelExecutionValidation(
    std::unique_ptr<optimization_guide::proto::ModelValidationInput> input) {
  if (!input) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &ModelValidatorKeyedService::PerformOnDeviceModelExecutionValidation,
          weak_ptr_factory_.GetWeakPtr(), std::move(input)),
      features::GetOnDeviceModelExecutionValidationStartupDelay());
}

void ModelValidatorKeyedService::PerformOnDeviceModelExecutionValidation(
    std::unique_ptr<optimization_guide::proto::ModelValidationInput> input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!opt_guide_service) {
    return;
  }
  if (!input || input->requests_size() == 0) {
    return;
  }
  // TODO: b/345495541 - Add support for conducting inference within a loop.
  // For now, we are just using the first request in the ModelValidationInput.
  auto request = input->requests(0);
  auto capability_key = ToModelBasedCapabilityKey(request.feature());

  on_device_validation_session_ =
      opt_guide_service->StartSession(capability_key,
                                      /*config_params=*/std::nullopt);
  auto metadata = GetProtoFromAny(request.request_metadata());
  on_device_validation_session_->AddContext(*metadata);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ModelValidatorKeyedService::ExecuteModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(metadata)),
      base::Seconds(30));
}

void ModelValidatorKeyedService::ExecuteModel(
    std::unique_ptr<google::protobuf::MessageLite> request_metadata) {
  DCHECK(on_device_validation_session_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request_metadata);
  on_device_validation_session_->ExecuteModel(
      *request_metadata,
      base::RepeatingCallback(base::BindRepeating(
          &ModelValidatorKeyedService::OnDeviceModelExecuteResponse,
          weak_ptr_factory_.GetWeakPtr())));
}

void ModelValidatorKeyedService::OnDeviceModelExecuteResponse(
    OptimizationGuideModelStreamingExecutionResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.response.has_value() || !result.response->is_complete) {
    return;
  }
  // Complete responses with empty log entry indicate errors.
  if (!result.log_entry || !result.provided_by_on_device) {
    LOCAL_HISTOGRAM_BOOLEAN(kModelValidationErrorHistogramString, true);
  }
  proto::ModelValidationOutput output;
  output.add_log_ai_data_requests()->CopyFrom(
      *result.log_entry->log_ai_data_request());

  auto out_file = switches::GetOnDeviceValidationWriteToFile();
  if (!out_file) {
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteResponseToFile, *out_file, output));
}

void ModelValidatorKeyedService::OnModelExecuteResponse(
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace optimization_guide
