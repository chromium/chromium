// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_validation.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace optimization_guide {

// Keyed service that validates models when enabled via command-line switch.
class ModelValidatorKeyedService : public KeyedService,
                                   public signin::IdentityManager::Observer {
 public:
  explicit ModelValidatorKeyedService(Profile* profile);
  ~ModelValidatorKeyedService() override;

 private:
  void StartModelExecutionValidation();

  // Kicks off the validation process for the on-device model.
  void StartOnDeviceModelExecutionValidation(
      std::unique_ptr<optimization_guide::proto::ModelValidationInput> input);

  // Calls the on-device model executor to execute the model.
  void PerformOnDeviceModelExecutionValidation(
      std::unique_ptr<optimization_guide::proto::ModelValidationInput> input);

  // Calls ExecuteModel on the on-device validation session.
  void ExecuteModel(
      std::unique_ptr<google::protobuf::MessageLite> request_metadata);

  // Invoked when model execution completes.
  void OnModelExecuteResponse(OptimizationGuideModelExecutionResult result,
                              std::unique_ptr<ModelQualityLogEntry> log_entry);

  // Invoked when on-device model execution completes.
  void OnDeviceModelExecuteResponse(
      OptimizationGuideModelStreamingExecutionResult result);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<OptimizationGuideModelExecutor::Session>
      on_device_validation_session_;

  base::ScopedObservation<signin::IdentityManager, ModelValidatorKeyedService>
      identity_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelValidatorKeyedService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_
