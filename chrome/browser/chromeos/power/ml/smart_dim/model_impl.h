// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/ml_service_client.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model.h"

namespace assist_ranker {
class ExamplePreprocessorConfig;
}  // namespace assist_ranker

namespace chromeos {
namespace power {
namespace ml {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmartDimModelResult {
  kSuccess = 0,
  kPreprocessorInitializationFailed = 1,
  kPreprocessorOtherError = 2,
  kOtherError = 3,
  kMismatchedFeatureSizeError = 4,
  kMlServiceInitializationFailedError = 5,
  kMaxValue = kMlServiceInitializationFailedError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmartDimParameterResult {
  kSuccess = 0,
  kUndefinedError = 1,
  kParsingError = 2,
  kUseDefaultValue = 3,
  kMaxValue = kUseDefaultValue
};

// Real implementation of SmartDimModel that predicts whether an upcoming screen
// dim should go ahead based on user activity/inactivity following dim.
class SmartDimModelImpl : public SmartDimModel {
 public:
  SmartDimModelImpl();
  ~SmartDimModelImpl() override;

  // chromeos::power::ml::SmartDimModel overrides:
  void RequestDimDecision(const UserActivityEvent::Features& features,
                          DimDecisionCallback dim_callback) override;
  void CancelPreviousRequest() override;

  // Override MlServiceClient in a unit test environment where there is no real
  // ML Service daemon to connect to.
  void SetMlServiceClientForTesting(std::unique_ptr<MlServiceClient> client);

 private:
  // Loads the preprocessor config if not already loaded. Also initializes the
  // MlServiceClient object.
  void LazyInitialize();

  // Pre-processes the input features into a vector, placed in
  // |*vectorized_features|, which is consumable by the ML model.
  //
  // Returns SmartDimModelResult::kSuccess on success, and the appropriate
  // error on failure.
  SmartDimModelResult PreprocessInput(
      const UserActivityEvent::Features& features,
      std::vector<float>* vectorized_features);

  // Takes an |inactivity_score| returned from the ML model and, using that,
  // returns a ModelPrediction.
  UserActivityEvent::ModelPrediction CreatePredictionFromInactivityScore(
      float inactivity_score);

  // Calls the ML service Mojo API to perform an Smart Dim inference call,
  // given |input_features|. The |callback| is supplied to the Mojo API,
  // which in turn will call it to provide the result (a ModelPrediction), once
  // the inference is complete.
  void ShouldDim(const UserActivityEvent::Features& input_features,
                 DimDecisionCallback callback);

  std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
      preprocessor_config_;

  // Cancelable wrapper for the DimDecisionCallback passed by the client to
  // RequestDimDecision().
  base::CancelableOnceCallback<void(UserActivityEvent::ModelPrediction)>
      cancelable_callback_;

  // Pointer to the object that handles the ML service calls.
  std::unique_ptr<MlServiceClient> ml_service_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SmartDimModelImpl);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
