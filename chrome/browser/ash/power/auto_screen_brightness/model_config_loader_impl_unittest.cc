// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/power/auto_screen_brightness/model_config_loader_impl.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

class TestObserver : public ModelConfigLoader::Observer {
 public:
  TestObserver() {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  // ModelConfigLoader::Observer overrides:
  void OnModelConfigLoaded(std::optional<ModelConfig> model_config) override {
    model_config_loader_initialized_ = true;
    model_config_ = model_config;
  }

  bool model_config_loader_initialized() const {
    return model_config_loader_initialized_;
  }
  std::optional<ModelConfig> model_config() { return model_config_; }

 private:
  bool model_config_loader_initialized_ = false;
  std::optional<ModelConfig> model_config_;
};

}  // namespace

class ModelConfigLoaderImplTest : public testing::Test {
 public:
  ModelConfigLoaderImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_params_path_ = temp_dir_.GetPath().Append("model_params.json");
  }

  ModelConfigLoaderImplTest(const ModelConfigLoaderImplTest&) = delete;
  ModelConfigLoaderImplTest& operator=(const ModelConfigLoaderImplTest&) =
      delete;

  ~ModelConfigLoaderImplTest() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  void Init(const std::string& model_params,
            const std::map<std::string, std::string>& experiment_params = {}) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (!experiment_params.empty()) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kAutoScreenBrightness, experiment_params);
    }

    WriteParamsToFile(model_params);
    model_config_loader_ = ModelConfigLoaderImpl::CreateForTesting(
        temp_params_path_, base::SequencedTaskRunner::GetCurrentDefault());

    test_observer_ = std::make_unique<TestObserver>();
    model_config_loader_->AddObserver(test_observer_.get());
    task_environment_.RunUntilIdle();
  }

 protected:
  void WriteParamsToFile(const std::string& params) {
    if (params.empty())
      return;

    CHECK(!temp_params_path_.empty());
    ASSERT_TRUE(base::WriteFile(temp_params_path_, params));
  }

  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  base::FilePath temp_params_path_;

  std::unique_ptr<ModelConfigLoaderImpl> model_config_loader_;
  std::unique_ptr<TestObserver> test_observer_;
};

TEST_F(ModelConfigLoaderImplTest, ValidModelParamsLoaded) {
  const std::string model_params = R"(
      {
        "auto_brightness_als_horizon_seconds": 2,
        "enabled": true,
        "global_curve": {
        "log_lux": [
            1.0,
            2.0,
            3.0
          ],
        "brightness": [
            10.0,
            20.0,
            30.0
          ]
         },
        "metrics_key": "abc",
        "model_als_horizon_seconds": 5
      }
      )";

  Init(model_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());

  std::vector<double> expected_log_lux = {1.0, 2.0, 3.0};
  std::vector<double> expected_brightness = {10.0, 20.0, 30.0};

  ModelConfig expected_model_config;
  expected_model_config.auto_brightness_als_horizon_seconds = 2.0;
  expected_model_config.enabled = true;
  expected_model_config.log_lux = expected_log_lux;
  expected_model_config.brightness = expected_brightness;
  expected_model_config.metrics_key = "abc";
  expected_model_config.model_als_horizon_seconds = 5;
  EXPECT_TRUE(test_observer_->model_config());
  EXPECT_EQ(*test_observer_->model_config(), expected_model_config);
}

TEST_F(ModelConfigLoaderImplTest, MissingEnabledMeansFalse) {
  const std::string model_params = R"(
      {
        "auto_brightness_als_horizon_seconds": 2,
        "global_curve": {
        "log_lux": [
            1.0,
            2.0,
            3.0
          ],
        "brightness": [
            10.0,
            20.0,
            30.0
          ]
         },
        "metrics_key": "abc",
        "model_als_horizon_seconds": 5
      }
      )";

  Init(model_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());

  std::vector<double> expected_log_lux = {1.0, 2.0, 3.0};
  std::vector<double> expected_brightness = {10.0, 20.0, 30.0};

  ModelConfig expected_model_config;
  expected_model_config.auto_brightness_als_horizon_seconds = 2.0;
  expected_model_config.enabled = false;
  expected_model_config.log_lux = expected_log_lux;
  expected_model_config.brightness = expected_brightness;
  expected_model_config.metrics_key = "abc";
  expected_model_config.model_als_horizon_seconds = 5;
  EXPECT_TRUE(test_observer_->model_config());
  EXPECT_EQ(*test_observer_->model_config(), expected_model_config);
}

TEST_F(ModelConfigLoaderImplTest, ValidModelParamsLoadedThenOverriden) {
  const std::string model_params = R"(
      {
        "auto_brightness_als_horizon_seconds": 2,
        "enabled": true,
        "global_curve": {
        "log_lux": [
            1.0,
            2.0,
            3.0
          ],
        "brightness": [
            10.0,
            20.0,
            30.0
          ]
         },
        "metrics_key": "abc",
        "model_als_horizon_seconds": 5
      }
      )";

  const std::string global_curve_spec("2:20,4:40,6:60");

  const std::map<std::string, std::string> experiment_params = {
      {"auto_brightness_als_horizon_seconds", "10"},
      {"enabled", "false"},
      {"model_als_horizon_seconds", "20"},
      {"global_curve", global_curve_spec},
  };

  Init(model_params, experiment_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());

  std::vector<double> expected_log_lux = {2.0, 4.0, 6.0};
  std::vector<double> expected_brightness = {20.0, 40.0, 60.0};

  ModelConfig expected_model_config;
  expected_model_config.auto_brightness_als_horizon_seconds = 10.0;
  expected_model_config.enabled = false;
  expected_model_config.log_lux = expected_log_lux;
  expected_model_config.brightness = expected_brightness;
  expected_model_config.metrics_key = "abc";
  expected_model_config.model_als_horizon_seconds = 20.0;
  EXPECT_TRUE(test_observer_->model_config());
  EXPECT_EQ(*test_observer_->model_config(), expected_model_config);
}

TEST_F(ModelConfigLoaderImplTest, InvalidModelParamsLoaded) {
  // "auto_brightness_als_horizon_seconds" is missing.
  const std::string model_params = R"(
      {
        "global_curve": {
        "log_lux": [
            1.0,
            2.0,
            3.0
          ],
        "brightness": [
            10.0,
            20.0,
            30.0
          ]
         },
        "metrics_key": "abc",
        "model_als_horizon_seconds": 5
      }
      )";

  Init(model_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());
  EXPECT_FALSE(test_observer_->model_config());
}

TEST_F(ModelConfigLoaderImplTest, InvalidModelParamsLoadedThenOverriden) {
  // Same as InvalidModelParamsLoaded, but missing
  // "auto_brightness_als_horizon_seconds" is specified in the experiment flags.
  const std::string model_params = R"(
      {
        "global_curve": {
        "log_lux": [
            1.0,
            2.0,
            3.0
          ],
        "brightness": [
            10.0,
            20.0,
            30.0
          ]
         },
        "metrics_key": "abc",
        "model_als_horizon_seconds": 5
      }
      )";

  const std::map<std::string, std::string> experiment_params = {
      {"auto_brightness_als_horizon_seconds", "10"},
      {"model_als_horizon_seconds", "20"},
  };

  Init(model_params, experiment_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());

  std::vector<double> expected_log_lux = {1.0, 2.0, 3.0};
  std::vector<double> expected_brightness = {10.0, 20.0, 30.0};

  ModelConfig expected_model_config;
  expected_model_config.auto_brightness_als_horizon_seconds = 10.0;
  expected_model_config.enabled = false;
  expected_model_config.log_lux = expected_log_lux;
  expected_model_config.brightness = expected_brightness;
  expected_model_config.metrics_key = "abc";
  expected_model_config.model_als_horizon_seconds = 20.0;
  EXPECT_TRUE(test_observer_->model_config());
  EXPECT_EQ(*test_observer_->model_config(), expected_model_config);
}

TEST_F(ModelConfigLoaderImplTest, MissingModelParams) {
  // Model params not found from disk and experiment flags do not contain
  // all fields we need.
  const std::map<std::string, std::string> experiment_params = {
      {"auto_brightness_als_horizon_seconds", "10"},
      {"model_als_horizon_seconds", "20"},
  };

  Init("" /* model_params */, experiment_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());
  EXPECT_FALSE(test_observer_->model_config());
}

TEST_F(ModelConfigLoaderImplTest, InvalidJsonFormat) {
  const std::string model_params = R"(
      {
        "global_curve": {
        "log_lux": [
            1.0,
            2.0,
            3.0
          ],
        "brightness": [
            10.0,
            20.0,
            30.0
          ]
         },
        "metrics_key": 10,
        "model_als_horizon_seconds": 5
      }
      )";

  const std::map<std::string, std::string> experiment_params = {
      {"auto_brightness_als_horizon_seconds", "10"},
      {"model_als_horizon_seconds", "20"},
  };

  Init(model_params, experiment_params);
  EXPECT_TRUE(test_observer_->model_config_loader_initialized());
  EXPECT_FALSE(test_observer_->model_config());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
