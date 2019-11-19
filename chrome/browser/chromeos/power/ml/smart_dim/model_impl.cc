// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/smart_dim/model_impl.h"

#include <cmath>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_helpers.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {

constexpr size_t k20181115ModelInputVectorSize = 343;
constexpr size_t k20190521ModelInputVectorSize = 592;

constexpr double k20181115ModelDefaultDimThreshold = -1.0;
constexpr double k20190521ModelDefaultDimThreshold = -0.6;

// Loads the preprocessor config protobuf, which will be used later to convert a
// RankerExample to a vectorized float for inactivity score calculation. Returns
// nullptr if cannot load or parse the config.
std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
LoadExamplePreprocessorConfig() {
  auto config = std::make_unique<assist_ranker::ExamplePreprocessorConfig>();

  const int res_id =
      base::FeatureList::IsEnabled(features::kSmartDimModelV3)
          ? IDR_SMART_DIM_20190521_EXAMPLE_PREPROCESSOR_CONFIG_PB
          : IDR_SMART_DIM_20181115_EXAMPLE_PREPROCESSOR_CONFIG_PB;

  scoped_refptr<base::RefCountedMemory> raw_config =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(res_id);
  if (!raw_config || !raw_config->front()) {
    LOG(ERROR) << "Failed to load SmartDimModel example preprocessor config.";
    return nullptr;
  }

  if (!config->ParseFromArray(raw_config->front(), raw_config->size())) {
    LOG(ERROR) << "Failed to parse SmartDimModel example preprocessor config.";
    return nullptr;
  }

  return config;
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

// Squashes |score| into range [0, 1] using sigmoid, and scales it to [0,100]
// so that values can be easily logged.
int ScoreToProbability(float score) {
  const float sigmoid = 1.0f / (1.0f + exp(-score));
  const int prob = floor(sigmoid * 100);
  return prob;
}

void LogPowerMLSmartDimModelResult(SmartDimModelResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimModel.Result", result);
}

void LogPowerMLSmartDimParameterResult(SmartDimParameterResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimParameter.Result", result);
}

// Returns "dim_threshold" from experiment parameter. Also logs status to UMA.
float GetDimThreshold() {
  const double default_threshold =
      base::FeatureList::IsEnabled(features::kSmartDimModelV3)
          ? k20190521ModelDefaultDimThreshold
          : k20181115ModelDefaultDimThreshold;
  const double dim_threshold = base::GetFieldTrialParamByFeatureAsDouble(
      features::kUserActivityPrediction, "dim_threshold", default_threshold);
  if (std::abs(dim_threshold - default_threshold) < 1e-10) {
    LogPowerMLSmartDimParameterResult(
        SmartDimParameterResult::kUseDefaultValue);
  } else {
    LogPowerMLSmartDimParameterResult(SmartDimParameterResult::kSuccess);
  }
  return dim_threshold;
}

}  // namespace

SmartDimModelImpl::SmartDimModelImpl() = default;

SmartDimModelImpl::~SmartDimModelImpl() = default;

SmartDimModelResult SmartDimModelImpl::PreprocessInput(
    const UserActivityEvent::Features& features,
    std::vector<float>* vectorized_features) {
  DCHECK(vectorized_features);
  LazyInitialize();

  if (!preprocessor_config_) {
    return SmartDimModelResult::kPreprocessorInitializationFailed;
  }

  assist_ranker::RankerExample ranker_example;
  if (!PopulateRankerExample(features, &ranker_example)) {
    return SmartDimModelResult::kOtherError;
  }

  int preprocessor_error = assist_ranker::ExamplePreprocessor::Process(
      *preprocessor_config_, &ranker_example, true);
  // kNoFeatureIndexFound can occur normally (e.g., when the domain name
  // isn't known to the model or a rarely seen enum value is used).
  if (preprocessor_error != assist_ranker::ExamplePreprocessor::kSuccess &&
      preprocessor_error !=
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

UserActivityEvent::ModelPrediction
SmartDimModelImpl::CreatePredictionFromInactivityScore(float inactivity_score) {
  UserActivityEvent::ModelPrediction prediction;
  const float dim_threshold = GetDimThreshold();

  prediction.set_decision_threshold(ScoreToProbability(dim_threshold));
  prediction.set_inactivity_score(ScoreToProbability(inactivity_score));

  if (inactivity_score >= dim_threshold) {
    prediction.set_response(UserActivityEvent::ModelPrediction::DIM);
  } else {
    prediction.set_response(UserActivityEvent::ModelPrediction::NO_DIM);
  }

  return prediction;
}

void SmartDimModelImpl::ShouldDim(
    const UserActivityEvent::Features& input_features,
    DimDecisionCallback callback) {
  UserActivityEvent::ModelPrediction prediction;
  prediction.set_response(UserActivityEvent::ModelPrediction::MODEL_ERROR);

  std::vector<float> vectorized_features;
  auto preprocess_result =
      PreprocessInput(input_features, &vectorized_features);
  if (preprocess_result != SmartDimModelResult::kSuccess) {
    LogPowerMLSmartDimModelResult(preprocess_result);
    std::move(callback).Run(prediction);
    return;
  }

  const size_t expected_size =
      base::FeatureList::IsEnabled(features::kSmartDimModelV3)
          ? k20190521ModelInputVectorSize
          : k20181115ModelInputVectorSize;

  if (vectorized_features.size() != expected_size) {
    LOG(ERROR) << "Smart Dim vectorized features not of correct size.";
    LogPowerMLSmartDimModelResult(
        SmartDimModelResult::kMismatchedFeatureSizeError);
    std::move(callback).Run(prediction);
    return;
  }

  if (!ml_service_client_) {
    LOG(ERROR) << "ML service Mojo client not initialized correctly";
    LogPowerMLSmartDimModelResult(
        SmartDimModelResult::kMlServiceInitializationFailedError);
    std::move(callback).Run(prediction);
    return;
  }

  ml_service_client_->DoInference(
      vectorized_features,
      base::Bind(&SmartDimModelImpl::CreatePredictionFromInactivityScore,
                 base::Unretained(this)),
      std::move(callback));
}

void SmartDimModelImpl::RequestDimDecision(
    const UserActivityEvent::Features& input_features,
    DimDecisionCallback dim_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel previously assigned callbacks and set it to the new callback.
  cancelable_callback_.Reset(std::move(dim_callback));
  ShouldDim(input_features, cancelable_callback_.callback());
}

void SmartDimModelImpl::CancelPreviousRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancelable_callback_.Cancel();
}

void SmartDimModelImpl::SetMlServiceClientForTesting(
    std::unique_ptr<MlServiceClient> client) {
  DCHECK(!ml_service_client_);
  ml_service_client_ = std::move(client);
}

void SmartDimModelImpl::LazyInitialize() {
  if (!ml_service_client_) {
    ml_service_client_ = CreateMlServiceClient();
  }

  if (!preprocessor_config_) {
    preprocessor_config_ = LoadExamplePreprocessorConfig();
  }
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
