// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/smart_dim/ml_service_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

using ::chromeos::machine_learning::mojom::BuiltinModelId;
using ::chromeos::machine_learning::mojom::BuiltinModelSpec;
using ::chromeos::machine_learning::mojom::BuiltinModelSpecPtr;
using ::chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using ::chromeos::machine_learning::mojom::ExecuteResult;
using ::chromeos::machine_learning::mojom::FloatList;
using ::chromeos::machine_learning::mojom::Int64List;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::Tensor;
using ::chromeos::machine_learning::mojom::TensorPtr;
using ::chromeos::machine_learning::mojom::ValueList;

namespace chromeos {
namespace power {
namespace ml {

namespace {

// TODO(crbug.com/893425): This should exist in only one location, so it should
// be merged with its duplicate in model_impl.cc to a common location.
void LogPowerMLSmartDimModelResult(SmartDimModelResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimModel.Result", result);
}

// Real impl of MlServiceClient.
class MlServiceClientImpl : public MlServiceClient {
 public:
  MlServiceClientImpl();
  ~MlServiceClientImpl() override {}

  // MlServiceClient:
  void DoInference(
      const std::vector<float>& features,
      base::RepeatingCallback<UserActivityEvent::ModelPrediction(float)>
          get_prediction_callback,
      SmartDimModel::DimDecisionCallback decision_callback) override;

 private:
  // Various callbacks that get invoked by the Mojo framework.
  void LoadModelCallback(
      ::chromeos::machine_learning::mojom::LoadModelResult result);
  void CreateGraphExecutorCallback(
      ::chromeos::machine_learning::mojom::CreateGraphExecutorResult result);

  // Callback executed by ML Service when an Execute call is complete.
  //
  // The |get_prediction_callback| and the |decision_callback| are bound
  // to the ExecuteCallback during while calling the Execute() function
  // on the Mojo API.
  void ExecuteCallback(
      base::RepeatingCallback<UserActivityEvent::ModelPrediction(float)>
          get_prediction_callback,
      SmartDimModel::DimDecisionCallback decision_callback,
      ::chromeos::machine_learning::mojom::ExecuteResult result,
      base::Optional<
          std::vector<::chromeos::machine_learning::mojom::TensorPtr>> outputs);
  // Initializes the various handles to the ML service if they're not already
  // available.
  void InitMlServiceHandlesIfNeeded();

  void OnMojoDisconnect();

  // Pointers used to execute functions in the ML service server end.
  ::chromeos::machine_learning::mojom::ModelPtr model_;
  mojo::Remote<::chromeos::machine_learning::mojom::GraphExecutor> executor_;

  base::WeakPtrFactory<MlServiceClientImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MlServiceClientImpl);
};

MlServiceClientImpl::MlServiceClientImpl() : MlServiceClient() {}

void MlServiceClientImpl::LoadModelCallback(LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    // TODO(crbug.com/893425): Log to UMA.
    LOG(ERROR) << "Failed to load Smart Dim model.";
  }
}

void MlServiceClientImpl::CreateGraphExecutorCallback(
    CreateGraphExecutorResult result) {
  if (result != CreateGraphExecutorResult::OK) {
    // TODO(crbug.com/893425): Log to UMA.
    LOG(ERROR) << "Failed to create Smart Dim Graph Executor.";
  }
}

void MlServiceClientImpl::ExecuteCallback(
    base::Callback<UserActivityEvent::ModelPrediction(float)>
        get_prediction_callback,
    SmartDimModel::DimDecisionCallback decision_callback,
    const ExecuteResult result,
    const base::Optional<std::vector<TensorPtr>> outputs) {
  UserActivityEvent::ModelPrediction prediction;

  if (result != ExecuteResult::OK) {
    LOG(ERROR) << "Smart Dim inference execution failed.";
    prediction.set_response(UserActivityEvent::ModelPrediction::MODEL_ERROR);
    LogPowerMLSmartDimModelResult(SmartDimModelResult::kOtherError);
  } else {
    float inactivity_score =
        (outputs.value())[0]->data->get_float_list()->value[0];
    prediction = get_prediction_callback.Run(inactivity_score);
    LogPowerMLSmartDimModelResult(SmartDimModelResult::kSuccess);
  }

  std::move(decision_callback).Run(prediction);
}

void MlServiceClientImpl::InitMlServiceHandlesIfNeeded() {
  if (!model_) {
    // Load the model.
    BuiltinModelSpecPtr spec = BuiltinModelSpec::New(
        base::FeatureList::IsEnabled(features::kSmartDimModelV3)
            ? BuiltinModelId::SMART_DIM_20190521
            : BuiltinModelId::SMART_DIM_20181115);
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->LoadBuiltinModel(
            std::move(spec), mojo::MakeRequest(&model_),
            base::BindOnce(&MlServiceClientImpl::LoadModelCallback,
                           weak_factory_.GetWeakPtr()));
  }

  if (!executor_) {
    // Get the graph executor.
    model_->CreateGraphExecutor(
        executor_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&MlServiceClientImpl::CreateGraphExecutorCallback,
                       weak_factory_.GetWeakPtr()));
    executor_.set_disconnect_handler(base::BindOnce(
        &MlServiceClientImpl::OnMojoDisconnect, weak_factory_.GetWeakPtr()));
  }
}

void MlServiceClientImpl::OnMojoDisconnect() {
  // TODO(crbug.com/893425): Log to UMA.
  LOG(WARNING) << "Mojo connection for ML service closed.";
  executor_.reset();
  model_.reset();
}

void MlServiceClientImpl::DoInference(
    const std::vector<float>& features,
    base::Callback<UserActivityEvent::ModelPrediction(float)>
        get_prediction_callback,
    SmartDimModel::DimDecisionCallback decision_callback) {
  InitMlServiceHandlesIfNeeded();

  // Prepare the input tensor.
  base::flat_map<std::string, TensorPtr> inputs;
  auto tensor = Tensor::New();
  tensor->shape = Int64List::New();
  tensor->shape->value = std::vector<int64_t>({1, features.size()});
  tensor->data = ValueList::New();
  tensor->data->set_float_list(FloatList::New());
  tensor->data->get_float_list()->value =
      std::vector<double>(std::begin(features), std::end(features));
  inputs.emplace(std::string("input"), std::move(tensor));

  std::vector<std::string> outputs({std::string("output")});

  executor_->Execute(
      std::move(inputs), std::move(outputs),
      base::BindOnce(&MlServiceClientImpl::ExecuteCallback,
                     weak_factory_.GetWeakPtr(), get_prediction_callback,
                     std::move(decision_callback)));
}

}  // namespace

std::unique_ptr<MlServiceClient> CreateMlServiceClient() {
  return std::make_unique<MlServiceClientImpl>();
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
