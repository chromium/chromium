// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/modeller_impl.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_brightness_monitor.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_light_provider.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_model_config_loader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chrome/browser/ash/power/auto_screen_brightness/trainer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

MonotoneCubicSpline CreateTestCurveFromTrainingData(
    const std::vector<TrainingDataPoint>& data) {
  CHECK_GT(data.size(), 1u);

  std::vector<double> xs;
  std::vector<double> ys;

  const auto& data_point = data[0];
  xs.push_back(data_point.ambient_log_lux);
  ys.push_back((data_point.brightness_old + data_point.brightness_new) / 2);

  for (size_t i = 1; i < data.size(); ++i) {
    xs.push_back(xs[i - 1] + 1);
    ys.push_back(ys[i - 1] + 1);
  }

  return *MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
}

void CheckOptionalCurves(
    const std::optional<MonotoneCubicSpline>& result_curve,
    const std::optional<MonotoneCubicSpline>& expected_curve) {
  EXPECT_EQ(result_curve.has_value(), expected_curve.has_value());
  if (result_curve) {
    EXPECT_EQ(*result_curve, *expected_curve);
  }
}

// Fake testing trainer that has configuration status and validity of personal
// curve specified in the constructor.
class FakeTrainer : public Trainer {
 public:
  FakeTrainer(bool is_configured,
              bool is_personal_curve_valid,
              bool return_new_curve,
              double curve_error)
      : is_configured_(is_configured),
        is_personal_curve_valid_(is_personal_curve_valid),
        return_new_curve_(return_new_curve),
        curve_error_(curve_error) {
    // If personal curve is valid, then the trainer must be configured.
    DCHECK(!is_personal_curve_valid_ || is_configured_);
  }

  FakeTrainer(const FakeTrainer&) = delete;
  FakeTrainer& operator=(const FakeTrainer&) = delete;

  ~FakeTrainer() override = default;

  // Trainer overrides:
  bool HasValidConfiguration() const override { return is_configured_; }

  bool SetInitialCurves(const MonotoneCubicSpline& global_curve,
                        const MonotoneCubicSpline& current_curve) override {
    DCHECK(is_configured_);
    global_curve_ = global_curve;
    current_curve_ = is_personal_curve_valid_ ? current_curve : global_curve;
    return is_personal_curve_valid_;
  }

  MonotoneCubicSpline GetGlobalCurve() const override {
    DCHECK(is_configured_);
    DCHECK(global_curve_);
    return *global_curve_;
  }

  MonotoneCubicSpline GetCurrentCurve() const override {
    DCHECK(is_configured_);
    DCHECK(current_curve_);
    return *current_curve_;
  }

  TrainingResult Train(const std::vector<TrainingDataPoint>& data) override {
    if (!return_new_curve_) {
      return TrainingResult(std::nullopt, curve_error_);
    }

    DCHECK(is_configured_);
    DCHECK(current_curve_);
    std::vector<TrainingDataPoint> used_data = data;

    // We need at least 2 points to create a MonotoneCubicSpline. Hence we
    // insert another one if |data| has only 1 point.
    if (data.size() == 1) {
      used_data.push_back(data[0]);
    }
    current_curve_ = CreateTestCurveFromTrainingData(used_data);
    return TrainingResult(current_curve_, curve_error_);
  }

 private:
  bool is_configured_;
  bool is_personal_curve_valid_;
  std::optional<MonotoneCubicSpline> global_curve_;
  std::optional<MonotoneCubicSpline> current_curve_;

  bool return_new_curve_ = false;
  double curve_error_ = 0.0;
};

class TestObserver : public Modeller::Observer {
 public:
  TestObserver() {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  // Modeller::Observer overrides:
  void OnModelTrained(const MonotoneCubicSpline& brightness_curve) override {
    model_.personal_curve = brightness_curve;
    ++model_.iteration_count;
  }

  void OnModelInitialized(const Model& model) override {
    model_initialized_ = true;
    model_ = model;
  }

  std::optional<MonotoneCubicSpline> personal_curve() const {
    return model_.personal_curve;
  }

  int iteration_count() const { return model_.iteration_count; }

  void CheckStatus(bool is_model_initialized, const Model& expected_model) {
    EXPECT_EQ(is_model_initialized, model_initialized_);
    CheckOptionalCurves(expected_model.global_curve, model_.global_curve);
    CheckOptionalCurves(expected_model.personal_curve, model_.personal_curve);
    EXPECT_EQ(expected_model.iteration_count, model_.iteration_count);
  }

 private:
  bool model_initialized_ = false;
  Model model_;
};

}  // namespace

class ModellerImplTest : public testing::Test {
 public:
  ModellerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_ = profile_builder.Build();
    test_model_config_ = GetTestModelConfig();
    test_initial_global_curve_ = MonotoneCubicSpline::CreateMonotoneCubicSpline(
        test_model_config_.log_lux, test_model_config_.brightness);
    DCHECK(test_initial_global_curve_);

    als_reader_ = std::make_unique<AlsReader>();
    fake_light_provider_ =
        std::make_unique<FakeLightProvider>(als_reader_.get());
  }

  ModellerImplTest(const ModellerImplTest&) = delete;
  ModellerImplTest& operator=(const ModellerImplTest&) = delete;

  ~ModellerImplTest() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  // Sets up |modeller_| with a FakeTrainer.
  void SetUpModeller(bool is_trainer_configured,
                     bool is_personal_curve_valid,
                     bool return_new_curve,
                     double curve_error) {
    modeller_ = ModellerImpl::CreateForTesting(
        profile_.get(), als_reader_.get(), &fake_brightness_monitor_,
        &fake_model_config_loader_, ui::UserActivityDetector::Get(),
        std::make_unique<FakeTrainer>(is_trainer_configured,
                                      is_personal_curve_valid, return_new_curve,
                                      curve_error),
        base::SequencedTaskRunner::GetCurrentDefault(),
        task_environment_.GetMockTickClock());

    test_observer_ = std::make_unique<TestObserver>();
    modeller_->AddObserver(test_observer_.get());
  }

  void Init(AlsReader::AlsInitStatus als_reader_status,
            BrightnessMonitor::Status brightness_monitor_status,
            std::optional<ModelConfig> model_config,
            bool is_trainer_configured = true,
            bool is_personal_curve_valid = true,
            bool return_new_curve = true,
            double curve_error = 0.0,
            const std::map<std::string, std::string>& params = {}) {
    if (!params.empty()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAutoScreenBrightness, params);
    }

    fake_light_provider_->set_als_init_status(als_reader_status);
    fake_brightness_monitor_.set_status(brightness_monitor_status);
    if (model_config) {
      fake_model_config_loader_.set_model_config(model_config.value());
    }

    SetUpModeller(is_trainer_configured, is_personal_curve_valid,
                  return_new_curve, curve_error);
    task_environment_.RunUntilIdle();
  }

 protected:
  void WriteModelToFile(const Model& model) {
    const ModellerImpl::ModelSavingSpec& model_saving_spec =
        ModellerImpl::ModellerImpl::GetModelSavingSpecFromProfilePath(
            profile_->GetPath());
    CHECK(!model_saving_spec.global_curve.empty());
    CHECK(!model_saving_spec.personal_curve.empty());
    CHECK(!model_saving_spec.iteration_count.empty());
    SaveModelToDisk(model_saving_spec, model, true /* save_global_curve */,
                    true /* save_personal_curve */, true /* is_testing */);
  }

  // Returns a valid ModelConfig.
  ModelConfig GetTestModelConfig() {
    ModelConfig model_config;
    model_config.auto_brightness_als_horizon_seconds = 2.0;
    model_config.log_lux = {
        3.69, 4.83, 6.54, 7.68, 8.25, 8.82,
    };
    model_config.brightness = {
        36.14, 47.62, 85.83, 93.27, 93.27, 100,
    };

    model_config.metrics_key = "abc";
    model_config.model_als_horizon_seconds = 3.0;
    return model_config;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;

  ModelConfig test_model_config_;
  std::optional<MonotoneCubicSpline> test_initial_global_curve_;

  std::unique_ptr<FakeLightProvider> fake_light_provider_;
  std::unique_ptr<AlsReader> als_reader_;
  FakeBrightnessMonitor fake_brightness_monitor_;
  FakeModelConfigLoader fake_model_config_loader_;

  std::unique_ptr<ModellerImpl> modeller_;
  std::unique_ptr<TestObserver> test_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// AlsReader is |kDisabled| when Modeller is created.
TEST_F(ModellerImplTest, AlsReaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kDisabled, BrightnessMonitor::Status::kSuccess,
       test_model_config_);

  // Model should be empty if modeller is disabled.
  test_observer_->CheckStatus(true /* is_model_initialized */, Model());
}

// BrightnessMonitor is |kDisabled| when Modeller is created.
TEST_F(ModellerImplTest, BrightnessMonitorDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kDisabled,
       test_model_config_);

  // Model should be empty if modeller is disabled.
  test_observer_->CheckStatus(true /* is_model_initialized */, Model());
}

// ModelConfigLoader has an invalid config, hence Modeller is disabled.
TEST_F(ModellerImplTest, ModelConfigLoaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       ModelConfig());

  // Model should be empty if modeller is disabled.
  test_observer_->CheckStatus(true /* is_model_initialized */, Model());
}

// AlsReader is |kDisabled| on later notification.
TEST_F(ModellerImplTest, AlsReaderDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess, test_model_config_);

  test_observer_->CheckStatus(false /* is_model_initialized */, Model());

  fake_light_provider_->set_als_init_status(
      AlsReader::AlsInitStatus::kDisabled);
  fake_light_provider_->ReportReaderInitialized();
  task_environment_.RunUntilIdle();

  // Model should be empty if modeller is disabled.
  test_observer_->CheckStatus(true /* is_model_initialized */, Model());
}

// AlsReader is |kSuccess| on later notification.
TEST_F(ModellerImplTest, AlsReaderEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess, test_model_config_);

  test_observer_->CheckStatus(false /* is_model_initialized */, Model());

  fake_light_provider_->set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_light_provider_->ReportReaderInitialized();
  task_environment_.RunUntilIdle();

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);
}

// BrightnessMonitor is |kDisabled| on later notification.
TEST_F(ModellerImplTest, BrightnessMonitorDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing, test_model_config_);

  test_observer_->CheckStatus(false /* is_model_initialized */, Model());

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kDisabled);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();

  // Model should be empty if modeller is disabled.
  test_observer_->CheckStatus(true /* is_model_initialized */, Model());
}

// BrightnessMonitor is |kSuccess| on later notification.
TEST_F(ModellerImplTest, BrightnessMonitorEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing, test_model_config_);

  test_observer_->CheckStatus(false /* is_model_initialized */, Model());

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  task_environment_.RunUntilIdle();

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);
}

// ModelConfigLoader reports an invalid config on later notification.
TEST_F(ModellerImplTest, InvalidModelConfigOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       std::nullopt /* model_config */);

  test_observer_->CheckStatus(false /* is_model_initialized */, Model());

  // ModelConfig() creates an invalid config.
  DCHECK(!IsValidModelConfig(ModelConfig()));
  fake_model_config_loader_.set_model_config(ModelConfig());
  fake_model_config_loader_.ReportModelConfigLoaded();
  task_environment_.RunUntilIdle();
  test_observer_->CheckStatus(true /* is_model_initialized */, Model());
}

// ModelConfigLoader reports a valid config on later notification.
TEST_F(ModellerImplTest, ValidModelConfigOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       std::nullopt /* model_config */);

  test_observer_->CheckStatus(false /* is_model_initialized */, Model());

  fake_model_config_loader_.set_model_config(test_model_config_);
  fake_model_config_loader_.ReportModelConfigLoaded();
  task_environment_.RunUntilIdle();

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);
}

// A model is loaded from disk, this is a personal curve, and the saved global
// curve is the same as initial global curve set from model config, hence there
// is no need to reset the model.
TEST_F(ModellerImplTest, ModelLoadedFromProfilePath) {
  const std::vector<double> xs = {0, 10, 20, 40, 60, 80, 90, 100};
  const std::vector<double> ys = {0, 5, 10, 15, 20, 25, 30, 40};
  const std::optional<MonotoneCubicSpline> personal_curve =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
  DCHECK(personal_curve);

  // Use |test_initial_global_curve_| as the saved global curve.
  const Model model(test_initial_global_curve_, personal_curve,
                    1 /* iteration_count */);
  WriteModelToFile(model);

  task_environment_.RunUntilIdle();

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_);
  task_environment_.RunUntilIdle();

  test_observer_->CheckStatus(true /* is_model_initialized */, model);
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.PersonalCurveValid", true, 1);
}

// A model is loaded from disk, this is a personal curve, and the saved global
// curve is different from the initial global curve set from model config, hence
// there the model is reset.
TEST_F(ModellerImplTest, ModelLoadedFromProfilePathWithReset) {
  const std::vector<double> xs = {0, 10, 20, 40, 60, 80, 90, 100};
  const std::vector<double> ys = {0, 5, 10, 15, 20, 25, 30, 40};
  const std::optional<MonotoneCubicSpline> saved_global_curve =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
  DCHECK(saved_global_curve);

  const Model model(saved_global_curve, saved_global_curve,
                    2 /* iteration_count */);
  WriteModelToFile(model);

  task_environment_.RunUntilIdle();

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_);

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.PersonalCurveValid", true, 1);
}

// A model is loaded from disk but the personal curve doesn't satisfy Trainer
// slope constraint, hence it's ignored and the global curve is used instead.
TEST_F(ModellerImplTest, PersonalCurveError) {
  const Model model(test_initial_global_curve_, test_initial_global_curve_, 2);
  WriteModelToFile(model);
  task_environment_.RunUntilIdle();

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       false /* is_personal_curve_valid */);

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.PersonalCurveValid", false, 1);
}

// Ambient light values are received. We check average ambient light has been
// calculated from the recent samples only.
TEST_F(ModellerImplTest, OnAmbientLightUpdated) {
  const int horizon_in_seconds = test_model_config_.model_als_horizon_seconds;

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid */);

  // No model is saved to disk, hence the initial model only has the global
  // curve set from the config.
  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  EXPECT_EQ(modeller_->GetModelConfigForTesting(), test_model_config_);

  const int first_lux = 1000;
  double running_sum = 0.0;
  for (int i = 0; i < horizon_in_seconds; ++i) {
    task_environment_.FastForwardBy(base::Seconds(1));
    const int lux = i == 0 ? first_lux : i;
    fake_light_provider_->ReportAmbientLightUpdate(lux);
    running_sum += ConvertToLog(lux);
    EXPECT_DOUBLE_EQ(
        modeller_->AverageAmbientForTesting(task_environment_.NowTicks())
            .value(),
        running_sum / (i + 1));
  }
  EXPECT_EQ(test_observer_->iteration_count(), 0);

  // Add another one should push the oldest |first_lux| out of the horizon.
  task_environment_.FastForwardBy(base::Seconds(1));
  fake_light_provider_->ReportAmbientLightUpdate(100);
  running_sum = running_sum + ConvertToLog(100) - ConvertToLog(first_lux);
  EXPECT_DOUBLE_EQ(
      modeller_->AverageAmbientForTesting(task_environment_.NowTicks()).value(),
      running_sum / horizon_in_seconds);
  EXPECT_EQ(test_observer_->iteration_count(), 0);
}

// User brightness changes are received, training example cache reaches
// |max_training_data_points_| to trigger early training. This all happens
// within a small window shorter than |training_delay_|.
TEST_F(ModellerImplTest, OnUserBrightnessChanged) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid */, true /* return_new_curve */,
       0.0 /* curve_error */,
       {{"training_delay_in_seconds", base::NumberToString(60)}});

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  std::vector<TrainingDataPoint> expected_data;

  for (size_t i = 0; i < modeller_->GetMaxTrainingDataPointsForTesting() - 1;
       ++i) {
    EXPECT_EQ(i, modeller_->NumberTrainingDataPointsForTesting());
    task_environment_.FastForwardBy(base::Milliseconds(1));
    const base::TimeTicks now = task_environment_.NowTicks();
    const int lux = i * 20;
    fake_light_provider_->ReportAmbientLightUpdate(lux);
    const double brightness_old = 10.0 + i;
    const double brightness_new = 20.0 + i;
    modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
    expected_data.push_back({brightness_old, brightness_new,
                             modeller_->AverageAmbientForTesting(now).value(),
                             now});
    EXPECT_EQ(test_observer_->iteration_count(), 0);
  }

  // Training should not have started.
  EXPECT_EQ(modeller_->GetMaxTrainingDataPointsForTesting() - 1,
            modeller_->NumberTrainingDataPointsForTesting());

  // Add one more data point to trigger the training early.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks now = task_environment_.NowTicks();
  const double brightness_old = 85;
  const double brightness_new = 95;
  modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
  expected_data.push_back({brightness_old, brightness_new,
                           modeller_->AverageAmbientForTesting(now).value(),
                           now});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_EQ(test_observer_->iteration_count(), 1);

  const std::optional<MonotoneCubicSpline>& result_curve =
      test_observer_->personal_curve();
  DCHECK(result_curve);

  const MonotoneCubicSpline expected_curve =
      CreateTestCurveFromTrainingData(expected_data);
  EXPECT_EQ(expected_curve, *result_curve);
}

// User activities resets timer used to start training.
TEST_F(ModellerImplTest, MultipleUserActivities) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid */, true /* return_new_curve */,
       0.0 /* curve_error */,
       {{"training_delay_in_seconds", base::NumberToString(60)}});

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  task_environment_.FastForwardBy(base::Seconds(1));
  fake_light_provider_->ReportAmbientLightUpdate(30);
  std::vector<TrainingDataPoint> expected_data;
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(i, modeller_->NumberTrainingDataPointsForTesting());
    task_environment_.FastForwardBy(base::Milliseconds(1));
    const base::TimeTicks now = task_environment_.NowTicks();
    const int lux = i * 20;
    fake_light_provider_->ReportAmbientLightUpdate(lux);
    const double brightness_old = 10.0 + i;
    const double brightness_new = 20.0 + i;
    modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
    expected_data.push_back({brightness_old, brightness_new,
                             modeller_->AverageAmbientForTesting(now).value(),
                             now});
    EXPECT_EQ(test_observer_->iteration_count(), 0);
  }

  EXPECT_EQ(modeller_->NumberTrainingDataPointsForTesting(), 10u);

  task_environment_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 2);
  // A user activity is received, timer should be reset.
  const ui::MouseEvent mouse_event(ui::EventType::kMouseExited,
                                   gfx::Point(0, 0), gfx::Point(0, 0),
                                   base::TimeTicks(), 0, 0);
  modeller_->OnUserActivity(&mouse_event);

  task_environment_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 3);
  EXPECT_EQ(modeller_->NumberTrainingDataPointsForTesting(), 10u);
  EXPECT_EQ(test_observer_->iteration_count(), 0);

  // Another user event is received.
  modeller_->OnUserActivity(&mouse_event);

  // After |training_delay_|/2, no training has started.
  task_environment_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 2);
  EXPECT_EQ(modeller_->NumberTrainingDataPointsForTesting(), 10u);
  EXPECT_EQ(test_observer_->iteration_count(), 0);

  // After another |training_delay_|/2, training is scheduled.
  task_environment_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 2);

  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_EQ(test_observer_->iteration_count(), 1);
  const std::optional<MonotoneCubicSpline>& result_curve =
      test_observer_->personal_curve();
  DCHECK(result_curve);

  const MonotoneCubicSpline expected_curve =
      CreateTestCurveFromTrainingData(expected_data);
  EXPECT_EQ(expected_curve, *result_curve);
}

// Training delay is 0, hence we train as soon as we have 1 data point.
TEST_F(ModellerImplTest, ZeroTrainingDelay) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid  */, true /* return_new_curve */,
       0.0 /* curve_error */,
       {
           {"training_delay_in_seconds", "0"},
       });

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  fake_light_provider_->ReportAmbientLightUpdate(30);
  const ui::MouseEvent mouse_event(ui::EventType::kMouseExited,
                                   gfx::Point(0, 0), gfx::Point(0, 0),
                                   base::TimeTicks(), 0, 0);
  modeller_->OnUserActivity(&mouse_event);

  modeller_->OnUserBrightnessChanged(10, 20);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_EQ(test_observer_->iteration_count(), 1);
}

// Curve is not updated and so model isn't exported.
TEST_F(ModellerImplTest, CurveUnchanged) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid  */, false /* return_new_curve */,
       0.0 /* curve_error */,
       {
           {"training_delay_in_seconds", "0"},
       });

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  fake_light_provider_->ReportAmbientLightUpdate(30);

  modeller_->OnUserBrightnessChanged(10, 20);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_EQ(test_observer_->iteration_count(), 0);
}

// Curve is updated but error is above the threshold, hence model isn't
// exported.
TEST_F(ModellerImplTest, CurveChangedLargeError) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid  */, true /* return_new_curve */,
       10.0 /* curve_error */,
       {
           {"training_delay_in_seconds", "0"},
           {"curve_error_tolerance", "5"},
       });

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  fake_light_provider_->ReportAmbientLightUpdate(30);

  modeller_->OnUserBrightnessChanged(10, 20);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_EQ(test_observer_->iteration_count(), 0);
}

// Curve is updated and error is not above the threshold, hence model is
// exported.
TEST_F(ModellerImplTest, CurveChangedSmallError) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       test_model_config_, true /* is_trainer_configured */,
       true /* is_personal_curve_valid  */, true /* return_new_curve */,
       10.0 /* curve_error */,
       {
           {"training_delay_in_seconds", "0"},
           {"curve_error_tolerance", "10"},
       });

  const Model expected_model(test_initial_global_curve_, std::nullopt, 0);
  test_observer_->CheckStatus(true /* is_model_initialized */, expected_model);

  fake_light_provider_->ReportAmbientLightUpdate(30);
  modeller_->OnUserBrightnessChanged(10, 20);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_EQ(test_observer_->iteration_count(), 1);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
