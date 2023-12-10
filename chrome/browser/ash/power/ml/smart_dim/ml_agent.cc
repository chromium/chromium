// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/ml_agent.h"

#include <cstddef>
#include <memory>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/power/ml/smart_dim/metrics.h"
#include "chrome/browser/ash/power/ml/smart_dim/ml_agent_util.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_helpers.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace ash {
namespace power {
namespace ml {

namespace {

using chromeos::machine_learning::mojom::ExecuteResult;
using chromeos::machine_learning::mojom::FloatList;
using chromeos::machine_learning::mojom::Int64List;
using chromeos::machine_learning::mojom::Tensor;
using chromeos::machine_learning::mojom::TensorPtr;
using chromeos::machine_learning::mojom::ValueList;

int ScoreToProbability(float score) {
  const float sigmoid = 1.0f / (1.0f + exp(-score));
  const int prob = floor(sigmoid * 100);
  return prob;
}

// Callback for completed ML Service calls to Execute() on a model's
// GraphExecutor.
void ExecuteCallback(const double threshold,
                     DimDecisionCallback decision_callback,
                     ExecuteResult result,
                     std::optional<std::vector<TensorPtr>> outputs) {
  UserActivityEvent::ModelPrediction prediction;

  if (result != ExecuteResult::OK) {
    DVLOG(1) << "Smart Dim inference execution failed.";
    prediction.set_response(UserActivityEvent::ModelPrediction::MODEL_ERROR);
    LogPowerMLSmartDimModelResult(SmartDimModelResult::kOtherError);
  } else {
    float inactivity_score =
        (outputs.value())[0]->data->get_float_list()->value[0];

    prediction.set_decision_threshold(ScoreToProbability(threshold));
    prediction.set_inactivity_score(ScoreToProbability(inactivity_score));
    prediction.set_response(inactivity_score >= threshold
                                ? UserActivityEvent::ModelPrediction::DIM
                                : UserActivityEvent::ModelPrediction::NO_DIM);

    LogPowerMLSmartDimModelResult(SmartDimModelResult::kSuccess);
  }

  std::move(decision_callback).Run(prediction);
}

// Populates |example| using |features|. Returns true if no error occurred.
bool PopulateRankerExample(const UserActivityEvent::Features& features,
                           assist_ranker::RankerExample* example) {
  CHECK(example);

  // Some features are bucketized before being logged to UKM. Hence training
  // examples use bucketized values. We need to bucketize them here to ensure
  // consistency.
  // It's ok if a feature is missing from |features|, and we will not return
  // false. But if a feature exists in |features|, then we expect it to have
  // a bucketized version in |buckets|. If its bucketized version is missing
  // from |buckets| then we return false.
  const std::map<std::string, int> buckets =
      UserActivityUkmLoggerBucketizer::BucketizeUserActivityEventFeatures(
          features);

  auto& ranker_example_features = *example->mutable_features();

  if (features.has_battery_percent()) {
    const auto it = buckets.find(kBatteryPercent);
    if (it == buckets.end())
      return false;
    ranker_example_features[kBatteryPercent].set_int32_value(it->second);
  }

  if (features.has_device_management()) {
    ranker_example_features["DeviceManagement"].set_int32_value(
        features.device_management());
  }

  if (features.has_device_mode()) {
    ranker_example_features["DeviceMode"].set_int32_value(
        features.device_mode());
  }

  if (features.has_device_type()) {
    ranker_example_features["DeviceType"].set_int32_value(
        features.device_type());
  }

  if (features.has_key_events_in_last_hour()) {
    const auto it = buckets.find(kKeyEventsInLastHour);
    if (it == buckets.end())
      return false;
    ranker_example_features[kKeyEventsInLastHour].set_int32_value(it->second);
  }

  if (features.has_last_activity_day()) {
    ranker_example_features["LastActivityDay"].set_int32_value(
        features.last_activity_day());
  }

  if (features.has_last_activity_time_sec()) {
    const auto it = buckets.find(kLastActivityTime);
    if (it == buckets.end())
      return false;
    ranker_example_features[kLastActivityTime].set_int32_value(it->second);
  }

  if (features.has_last_user_activity_time_sec()) {
    const auto it = buckets.find(kLastUserActivityTime);
    if (it == buckets.end())
      return false;
    ranker_example_features[kLastUserActivityTime].set_int32_value(it->second);
  }

  if (features.has_mouse_events_in_last_hour()) {
    const auto it = buckets.find(kMouseEventsInLastHour);
    if (it == buckets.end())
      return false;
    ranker_example_features[kMouseEventsInLastHour].set_int32_value(it->second);
  }

  if (features.has_on_battery()) {
    // This is an int value in the model.
    ranker_example_features["OnBattery"].set_int32_value(features.on_battery());
  }

  ranker_example_features["PreviousNegativeActionsCount"].set_int32_value(
      features.previous_negative_actions_count());
  ranker_example_features["PreviousPositiveActionsCount"].set_int32_value(
      features.previous_positive_actions_count());

  ranker_example_features["RecentTimeActive"].set_int32_value(
      features.recent_time_active_sec());

  if (features.has_video_playing_time_sec()) {
    const auto it = buckets.find(kRecentVideoPlayingTime);
    if (it == buckets.end())
      return false;
    ranker_example_features[kRecentVideoPlayingTime].set_int32_value(
        it->second);
  }

  if (features.has_on_to_dim_sec()) {
    ranker_example_features["ScreenDimDelay"].set_int32_value(
        features.on_to_dim_sec());
  }

  if (features.has_dim_to_screen_off_sec()) {
    ranker_example_features["ScreenDimToOffDelay"].set_int32_value(
        features.dim_to_screen_off_sec());
  }

  if (features.has_time_since_last_key_sec()) {
    ranker_example_features["TimeSinceLastKey"].set_int32_value(
        features.time_since_last_key_sec());
  }

  if (features.has_time_since_last_mouse_sec()) {
    ranker_example_features["TimeSinceLastMouse"].set_int32_value(
        features.time_since_last_mouse_sec());
  }

  if (features.has_time_since_video_ended_sec()) {
    const auto it = buckets.find(kTimeSinceLastVideoEnded);
    if (it == buckets.end())
      return false;
    ranker_example_features[kTimeSinceLastVideoEnded].set_int32_value(
        it->second);
  }

  if (features.has_engagement_score()) {
    ranker_example_features["SiteEngagementScore"].set_int32_value(
        features.engagement_score());
  }

  if (features.has_has_form_entry()) {
    ranker_example_features["HasFormEntry"].set_bool_value(
        features.has_form_entry());
  }

  if (features.has_tab_domain()) {
    ranker_example_features["TabDomain"].set_string_value(
        features.tab_domain());
    ranker_example_features["HasTabs"].set_bool_value(true);
  } else {
    ranker_example_features["HasTabs"].set_bool_value(false);
  }

  return true;
}

// Vectorize the features proto to feature vector with preprocessor.
SmartDimModelResult PreprocessInput(
    const assist_ranker::ExamplePreprocessorConfig& preprocessor_config,
    const UserActivityEvent::Features& features,
    std::vector<float>* vectorized_features) {
  DCHECK(vectorized_features);

  assist_ranker::RankerExample ranker_example;
  if (!PopulateRankerExample(features, &ranker_example)) {
    return SmartDimModelResult::kOtherError;
  }

  int preprocessor_result = assist_ranker::ExamplePreprocessor::Process(
      preprocessor_config, &ranker_example, true);
  // kNoFeatureIndexFound can occur normally (e.g., when the domain name
  // isn't known to the model or a rarely seen enum value is used).
  if (preprocessor_result != assist_ranker::ExamplePreprocessor::kSuccess &&
      preprocessor_result !=
          assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound) {
    return SmartDimModelResult::kPreprocessorOtherError;
  }
  const auto& extracted_features =
      ranker_example.features()
          .at(assist_ranker::ExamplePreprocessor::kVectorizedFeatureDefaultName)
          .float_list()
          .float_value();
  vectorized_features->assign(extracted_features.begin(),
                              extracted_features.end());

  return SmartDimModelResult::kSuccess;
}

}  // namespace

SmartDimMlAgent::SmartDimMlAgent() = default;

SmartDimMlAgent::~SmartDimMlAgent() = default;

SmartDimMlAgent* SmartDimMlAgent::GetInstance() {
  static base::NoDestructor<SmartDimMlAgent> smart_dim_ml_agent;
  return smart_dim_ml_agent.get();
}

bool SmartDimMlAgent::IsDownloadWorkerReady() {
  return download_worker_.IsReady();
}

void SmartDimMlAgent::OnComponentReady(const ComponentFileContents& contents) {
  download_worker_.InitializeFromComponent(std::move(contents));
}

void SmartDimMlAgent::RequestDimDecision(
    const UserActivityEvent::Features& features,
    DimDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dim_decision_callback_.Reset(std::move(callback));

  auto* worker = GetWorker();

  UserActivityEvent::ModelPrediction prediction;
  prediction.set_response(UserActivityEvent::ModelPrediction::MODEL_ERROR);

  DCHECK(worker->GetPreprocessorConfig());
  std::vector<float> vectorized_features;
  auto preprocess_result = PreprocessInput(*(worker->GetPreprocessorConfig()),
                                           features, &vectorized_features);
  if (preprocess_result != SmartDimModelResult::kSuccess) {
    LogPowerMLSmartDimModelResult(preprocess_result);
    dim_decision_callback_.callback().Run(prediction);
    return;
  }

  if (vectorized_features.size() != worker->expected_feature_size()) {
    DVLOG(1) << "Smart Dim vectorized features not of correct size.";
    LogPowerMLSmartDimModelResult(
        SmartDimModelResult::kMismatchedFeatureSizeError);
    dim_decision_callback_.callback().Run(prediction);
    return;
  }

  DCHECK(worker->GetExecutor());
  // Prepare the input tensor.
  base::flat_map<std::string, TensorPtr> inputs;
  auto tensor = Tensor::New();
  tensor->shape = Int64List::New();
  tensor->shape->value = std::vector<int64_t>(
      {1, static_cast<int64_t>(vectorized_features.size())});
  tensor->data = ValueList::NewFloatList(FloatList::New(std::vector<double>(
      std::begin(vectorized_features), std::end(vectorized_features))));
  inputs.emplace(std::string(kSmartDimInputNodeName), std::move(tensor));

  std::vector<std::string> outputs({std::string(kSmartDimOutputNodeName)});

  // Gets dim_threshold from finch experiment parameter, also logs status to
  // UMA.
  const double dim_threshold = base::GetFieldTrialParamByFeatureAsDouble(
      features::kUserActivityPrediction, "dim_threshold",
      worker->dim_threshold());
  if (std::abs(dim_threshold - worker->dim_threshold()) < 1e-10)
    LogPowerMLSmartDimParameterResult(
        SmartDimParameterResult::kUseDefaultValue);
  else
    LogPowerMLSmartDimParameterResult(SmartDimParameterResult::kSuccess);

  worker->GetExecutor()->Execute(
      std::move(inputs), std::move(outputs),
      base::BindOnce(&ExecuteCallback, dim_threshold,
                     dim_decision_callback_.callback()));
}

void SmartDimMlAgent::CancelPreviousRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dim_decision_callback_.Cancel();
}

void SmartDimMlAgent::ResetForTesting() {
  builtin_worker_.Reset();
  download_worker_.Reset();
}

SmartDimWorker* SmartDimMlAgent::GetWorker() {
  if (download_worker_.IsReady()) {
    // When download_worker_ is ready, builtin_worker_ is not useful any more,
    // we can release it to save memory.
    builtin_worker_.Reset();
    LogWorkerType(WorkerType::kDownloadWorker);
    return &download_worker_;
  }
  LogWorkerType(WorkerType::kBuiltinWorker);
  return &builtin_worker_;
}

}  // namespace ml
}  // namespace power
}  // namespace ash
