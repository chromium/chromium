// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config_loader_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// Reads string content from |model_params_path|, which should exist.
// This should run in another thread to be non-blocking to the main thread (if
// |is_testing| is false).
std::string LoadModelParamsFromDisk(const base::FilePath& model_params_path,
                                    bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(base::PathExists(model_params_path));

  std::string content;
  if (!base::ReadFileToString(model_params_path, &content)) {
    return std::string();
  }
  return content;
}

// ModelConfigFromJson and GlobalCurveFromJson are used to hold JSON-parsed
// params.
struct GlobalCurveFromJson {
  std::vector<std::unique_ptr<double>> log_lux;
  std::vector<std::unique_ptr<double>> brightness;
  static void RegisterJSONConverter(
      base::JSONValueConverter<GlobalCurveFromJson>* converter) {
    converter->RegisterRepeatedDouble("log_lux", &GlobalCurveFromJson::log_lux);
    converter->RegisterRepeatedDouble("brightness",
                                      &GlobalCurveFromJson::brightness);
  }
};

struct ModelConfigFromJson {
  double auto_brightness_als_horizon_seconds;
  bool enabled;
  GlobalCurveFromJson global_curve;
  std::string metrics_key;
  double model_als_horizon_seconds;

  ModelConfigFromJson()
      : auto_brightness_als_horizon_seconds(-1.0),
        enabled(false),
        model_als_horizon_seconds(-1.0) {}

  static void RegisterJSONConverter(
      base::JSONValueConverter<ModelConfigFromJson>* converter) {
    converter->RegisterDoubleField(
        "auto_brightness_als_horizon_seconds",
        &ModelConfigFromJson::auto_brightness_als_horizon_seconds);
    converter->RegisterBoolField("enabled", &ModelConfigFromJson::enabled);
    converter->RegisterNestedField("global_curve",
                                   &ModelConfigFromJson::global_curve);
    converter->RegisterStringField("metrics_key",
                                   &ModelConfigFromJson::metrics_key);
    converter->RegisterDoubleField(
        "model_als_horizon_seconds",
        &ModelConfigFromJson::model_als_horizon_seconds);
  }
};

}  // namespace

ModelConfigLoaderImpl::ModelConfigLoaderImpl()
    : ModelConfigLoaderImpl(
          base::FilePath(
              "/usr/share/chromeos-assets/autobrightness/model_params.json"),
          base::CreateSequencedTaskRunner(
              {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
               base::MayBlock(),
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
          false /* is_testing */) {}

ModelConfigLoaderImpl::~ModelConfigLoaderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ModelConfigLoaderImpl::AddObserver(ModelConfigLoader::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (is_initialized_) {
    observer->OnModelConfigLoaded(
        is_model_config_valid_ ? base::Optional<ModelConfig>(model_config_)
                               : base::nullopt);
  }
}

void ModelConfigLoaderImpl::RemoveObserver(
    ModelConfigLoader::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ModelConfigLoaderImpl> ModelConfigLoaderImpl::CreateForTesting(
    const base::FilePath& model_params_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  return base::WrapUnique(new ModelConfigLoaderImpl(
      model_params_path, blocking_task_runner, true /* is_testing */));
}

ModelConfigLoaderImpl::ModelConfigLoaderImpl(
    const base::FilePath& model_params_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    bool is_testing)
    : model_params_path_(model_params_path),
      blocking_task_runner_(blocking_task_runner),
      is_testing_(is_testing) {
  Init();
}

void ModelConfigLoaderImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::PathExists(model_params_path_)) {
    // Allow experiment flags to provide configs if there isn't any config from
    // the disk.
    InitFromParams();
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadModelParamsFromDisk, model_params_path_, is_testing_),
      base::BindOnce(&ModelConfigLoaderImpl::OnModelParamsLoadedFromDisk,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModelConfigLoaderImpl::InitFromParams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  model_config_.auto_brightness_als_horizon_seconds =
      GetFieldTrialParamByFeatureAsInt(
          features::kAutoScreenBrightness,
          "auto_brightness_als_horizon_seconds",
          model_config_.auto_brightness_als_horizon_seconds);

  model_config_.enabled = GetFieldTrialParamByFeatureAsBool(
      features::kAutoScreenBrightness, "enabled", model_config_.enabled);

  model_config_.model_als_horizon_seconds = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "model_als_horizon_seconds",
      model_config_.model_als_horizon_seconds);

  const std::string global_curve = GetFieldTrialParamValueByFeature(
      features::kAutoScreenBrightness, "global_curve");

  if (global_curve.empty()) {
    OnInitializationComplete();
    return;
  }

  base::StringPairs key_value_pairs;
  if (!base::SplitStringIntoKeyValuePairs(global_curve, ':', ',',
                                          &key_value_pairs)) {
    OnInitializationComplete();
    return;
  }

  std::vector<double> log_lux;
  std::vector<double> brightness;

  for (const auto& key_value : key_value_pairs) {
    double log_lux_val = 0;
    if (!base::StringToDouble(key_value.first, &log_lux_val)) {
      OnInitializationComplete();
      return;
    }

    double brightness_val = 0;
    if (!base::StringToDouble(key_value.second, &brightness_val)) {
      OnInitializationComplete();
      return;
    }

    log_lux.push_back(log_lux_val);
    brightness.push_back(brightness_val);
  }

  model_config_.log_lux = log_lux;
  model_config_.brightness = brightness;

  OnInitializationComplete();
}

void ModelConfigLoaderImpl::OnModelParamsLoadedFromDisk(
    const std::string& content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (content.empty()) {
    InitFromParams();
    return;
  }

  base::Optional<base::Value> value = base::JSONReader::Read(content);
  if (!value) {
    InitFromParams();
    return;
  }

  base::JSONValueConverter<ModelConfigFromJson> converter;
  ModelConfigFromJson loaded_model_configs;
  if (!converter.Convert(*value, &loaded_model_configs)) {
    InitFromParams();
    return;
  }

  model_config_.auto_brightness_als_horizon_seconds =
      loaded_model_configs.auto_brightness_als_horizon_seconds;

  model_config_.enabled = loaded_model_configs.enabled;

  std::vector<double> log_lux;
  for (const auto& log_lux_val : loaded_model_configs.global_curve.log_lux) {
    DCHECK(log_lux_val);
    model_config_.log_lux.push_back(*log_lux_val);
  }

  std::vector<double> brightness;
  for (const auto& brightness_val :
       loaded_model_configs.global_curve.brightness) {
    DCHECK(brightness_val);
    model_config_.brightness.push_back(*brightness_val);
  }

  model_config_.metrics_key = loaded_model_configs.metrics_key;
  model_config_.model_als_horizon_seconds =
      loaded_model_configs.model_als_horizon_seconds;

  InitFromParams();
}

void ModelConfigLoaderImpl::OnInitializationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_model_config_valid_ = IsValidModelConfig(model_config_);
  is_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnModelConfigLoaded(
        is_model_config_valid_ ? base::Optional<ModelConfig>(model_config_)
                               : base::nullopt);
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
