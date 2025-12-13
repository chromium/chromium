// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_ML_AGENT_H_
#define CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_ML_AGENT_H_

#include <optional>

#include "base/cancelable_callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/power/ml/smart_dim/builtin_worker.h"
#include "chrome/browser/ash/power/ml/smart_dim/download_worker.h"
#include "chrome/browser/ash/power/ml/smart_dim/smart_dim_worker.h"
#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"

namespace ash {
namespace power {
namespace ml {

using DimDecisionCallback =
    base::OnceCallback<void(std::optional<UserActivityEvent::ModelPrediction>)>;

// SmartDimMlAgent is responsible for preprocessing the features and requesting
// the inference from machine learning service.
// Usage:
//
//     SmartDimMlAgent::GetInstance()->RequestDimDecision(
//         features_, dim_decision_callback);
class SmartDimMlAgent {
 public:
  static SmartDimMlAgent* GetInstance();

  SmartDimMlAgent(const SmartDimMlAgent&) = delete;
  SmartDimMlAgent& operator=(const SmartDimMlAgent&) = delete;

  // Post a request to determine whether an upcoming dim should go ahead based
  // on input |features|. When a decision is arrived at, it is returned via
  // |callback|.
  //
  // If this method is called again before the |callback| from a previous call
  // has completed, or if the dim decision is explicitly canceled via
  // |CancelPreviousRequest()| below, the previous |callback| will be invoked
  // with an empty optional<ModelPrediction>.
  void RequestDimDecision(const UserActivityEvent::Features& features,
                          DimDecisionCallback callback);
  void CancelPreviousRequest();

  // Called by CUS(component update service). When new version of the component
  // downloaded, CUS first uses IsDownloadWorkerReady to see if download worker
  // is ready. If it's not, CUS then uses OnComponentReady to update the
  // download metainfo, preprocessor and model.
  bool IsDownloadWorkerReady();
  void OnComponentReady(const ComponentFileContents& contents);

  // Called by ml_agent_unittest.cc to reset the builtin and download worker.
  void ResetForTesting();

  DownloadWorker* download_worker_for_test() { return &download_worker_; }

 protected:
  SmartDimMlAgent();
  virtual ~SmartDimMlAgent();

 private:
  friend base::NoDestructor<SmartDimMlAgent>;

  // Return download_worker_ if it's ready, otherwise builtin_worker_.
  SmartDimWorker* GetWorker();

  BuiltinWorker builtin_worker_;
  DownloadWorker download_worker_;

  base::CancelableOnceCallback<void(
      std::optional<UserActivityEvent::ModelPrediction>)>
      cancelable_dim_decision_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_ML_AGENT_H_
