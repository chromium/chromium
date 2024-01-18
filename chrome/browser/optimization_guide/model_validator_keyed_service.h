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

  // Invoked when model execution completes.
  void OnModelExecuteResponse(OptimizationGuideModelExecutionResult result,
                              std::unique_ptr<ModelQualityLogEntry> log_entry);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  raw_ptr<Profile> profile_;

  base::ScopedObservation<signin::IdentityManager, ModelValidatorKeyedService>
      identity_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelValidatorKeyedService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_
