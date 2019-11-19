// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_SERVICE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_SERVICE_CLIENT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model.h"

namespace chromeos {
namespace power {
namespace ml {

// ML service Mojo client which loads a Smart Dim model, and then performs
// inference on inputs provided by the caller.
class MlServiceClient {
 public:
  virtual ~MlServiceClient() = default;

  // Sends an input vector to the ML service to run inference on. It also
  // provides a |decision_callback| to the Mojo service which will be run
  // by the ExecuteCallback() on a return from the inference call.
  //
  // |get_prediction_callback| takes an inactivity score value returned
  // by the ML model and uses it to return a ModelPrediction which is fed
  // into the |decision_callback|.
  //
  // NOTE: A successful Mojo call *does not* guarantee a successful inference
  // call. The ExecuteCallback can be run with failure result, in case the
  // inference call failed.
  virtual void DoInference(
      const std::vector<float>& features,
      base::RepeatingCallback<UserActivityEvent::ModelPrediction(float)>
          get_prediction_callback,
      SmartDimModel::DimDecisionCallback decision_callback) = 0;
};

std::unique_ptr<MlServiceClient> CreateMlServiceClient();

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_SERVICE_CLIENT_H_
