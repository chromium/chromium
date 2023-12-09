// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/model_config_loader_impl.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

// Model params are usually written to this path, unless it is a unibuild.
constexpr char kDefaultModelParamsPath[] =
    "/usr/share/chromeos-assets/autobrightness/model_params.json";

// Model params for unibuild devices will be written to device-specific
// directories. We need to read the relevant directory from the following system
// file path.
constexpr char kSystemPath[] =
    "/run/chromeos-config/v1/power/autobrightness/config-file/system-path";

// Reads string content from |model_params_path| if it exists.
// This should run in another thread to be non-blocking to the main thread (if
// |is_testing| is false).
std::string LoadModelParamsFromDisk(const base::FilePath& model_params_path,
                                    bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::FilePath used_model_params_path = model_params_path;
  if (used_model_params_path.empty()) {
    // Only override supplied path if it is empty.
    const base::FilePath default_path = base::FilePath(kDefaultModelParamsPath);
    if (base::PathExists(default_path)) {
      // If default path exists, use it.
      used_model_params_path = default_path;
    } else {
      // If default path doesn't exist, try to get the path from the system
      // file.
      const base::FilePath system_path = base::FilePath(kSystemPath);
      if (base::PathExists(system_path)) {
        std::string dest_path_value;
        if (base::ReadFileToString(system_path, &dest_path_value)) {
          used_model_params_path = base::FilePath(dest_path_value);
        }
      }
    }
  }

  if (used_model_params_path.empty() ||
      !base::PathExists(used_model_params_path)) {
    VLOG(1) << "ABLoader model params path does not exist";
    return std::string();
  }

  std::string content;
  if (!base::ReadFileToString(used_model_params_path, &content)) {
    VLOG(1) << "ABLoader cannot load params from " << used_model_params_path;
    return std::string();
  }
  VLOG(1) << "ABLoader successfully loaded params from "
          << used_model_params_path;
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
          base::FilePath(),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
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
        is_model_config_valid_ ? std::optional<ModelConfig>(model_config_)
                               : std::nullopt);
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
    : blocking_task_runner_(blocking_task_runner), is_testing_(is_testing) {
  Init(model_params_path);
}

void ModelConfigLoaderImpl::Init(const base::FilePath& model_params_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadModelParamsFromDisk, model_params_path, is_testing_),
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

  std::optional<base::Value> value = base::JSONReader::Read(content);
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
    observer.OnModelConfigLoaded(is_model_config_valid_
                                     ? std::optional<ModelConfig>(model_config_)
                                     : std::nullopt);
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
