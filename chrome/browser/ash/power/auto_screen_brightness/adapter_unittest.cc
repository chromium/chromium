// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/adapter.h"

#include <map>
#include <numeric>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_brightness_monitor.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_light_provider.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_model_config_loader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/modeller.h"
#include "chrome/browser/ash/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

// Checks |actual_avg_log| is equal to the avg log calculated from
// |expected_data|. |expected_data| contains absolute lux value, not log lux.
void CheckAvgLog(const std::vector<double>& expected_data,
                 double actual_avg_log) {
  const size_t count = expected_data.size();
  CHECK_NE(count, 0u);
  const double expected_avg_log =
      std::accumulate(
          expected_data.begin(), expected_data.end(), 0.0,
          [](double sum, double lux) { return sum + ConvertToLog(lux); }) /
      count;
  EXPECT_DOUBLE_EQ(actual_avg_log, expected_avg_log);
}

// Testing modeller.
class FakeModeller : public Modeller {
 public:
  FakeModeller() = default;
  ~FakeModeller() override = default;

  void InitModellerWithModel(const Model& model) {
    DCHECK(!modeller_initialized_);
    modeller_initialized_ = true;
    model_ = model;
  }

  void ReportModelTrained(const MonotoneCubicSpline& personal_curve) {
    DCHECK(modeller_initialized_);
    model_.personal_curve = personal_curve;
    for (auto& observer : observers_)
      observer.OnModelTrained(personal_curve);
  }

  void ReportModelInitialized() {
    DCHECK(modeller_initialized_);
    for (auto& observer : observers_)
      observer.OnModelInitialized(model_);
  }

  // Modeller overrides:
  void AddObserver(Modeller::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
    if (modeller_initialized_)
      observer->OnModelInitialized(model_);
  }

  void RemoveObserver(Modeller::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

 private:
  bool modeller_initialized_ = false;
  Model model_;

  base::ObserverList<Observer> observers_;
};

class TestObserver : public chromeos::PowerManagerClient::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // chromeos::PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override {
    ++num_changes_;
    change_ = change;
  }
  double GetBrightnessPercent() const { return change_.percent(); }

  int num_changes() const { return num_changes_; }

  power_manager::BacklightBrightnessChange_Cause GetCause() const {
    return change_.cause();
  }

 private:
  int num_changes_ = 0;
  power_manager::BacklightBrightnessChange change_;
};

}  // namespace

// TODO(jiameng): add more unit tests on AdapterDecision related histograms.
class AdapterTest : public testing::Test {
 public:
  AdapterTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  AdapterTest(const AdapterTest&) = delete;
  AdapterTest& operator=(const AdapterTest&) = delete;

  ~AdapterTest() override = default;

  void SetUp() override {
    als_reader_ = std::make_unique<AlsReader>();
    fake_light_provider_ =
        std::make_unique<FakeLightProvider>(als_reader_.get());

    chromeos::PowerManagerClient::InitializeFake();
    power_manager::SetBacklightBrightnessRequest request;
    request.set_percent(1);
    chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
    task_environment_.RunUntilIdle();

    chromeos::PowerManagerClient::Get()->AddObserver(&test_observer_);

    global_curve_ = MonotoneCubicSpline::CreateMonotoneCubicSpline(
        {-4, 12, 20}, {30, 80, 100});
    personal_curve_ = MonotoneCubicSpline::CreateMonotoneCubicSpline(
        {-4, 12, 20}, {20, 60, 100});
    DCHECK(global_curve_);
    DCHECK(personal_curve_);
  }

  void TearDown() override {
    adapter_.reset();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    chromeos::PowerManagerClient::Shutdown();
  }

  // Creates Adapter only, but its input may or may not be ready.
  void SetUpAdapter(const std::map<std::string, std::string>& params,
                    bool brightness_set_by_policy = false) {
    // Simulate the real clock that will not produce TimeTicks equal to 0.
    // This is because the Adapter will treat 0 TimeTicks are uninitialized
    // values.
    task_environment_.FastForwardBy(base::Seconds(1));
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_user_prefs(base::WrapRefCounted(new TestingPrefStore()));
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);

    auto_screen_brightness::MetricsReporter::RegisterLocalStatePrefs(
        registry.get());

    sync_preferences::PrefServiceSyncable* regular_prefs =
        factory.CreateSyncable(registry.get()).release();

    RegisterUserProfilePrefs(registry.get());
    if (brightness_set_by_policy) {
      regular_prefs->SetInteger(ash::prefs::kPowerAcScreenBrightnessPercent,
                                10);
      regular_prefs->SetInteger(
          ash::prefs::kPowerBatteryScreenBrightnessPercent, 10);
    }

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPrefService(base::WrapUnique(regular_prefs));

    profile_ = profile_builder.Build();

    if (!params.empty()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAutoScreenBrightness, params);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutoScreenBrightness);
    }

    adapter_ = Adapter::CreateForTesting(
        profile_.get(), als_reader_.get(), &fake_brightness_monitor_,
        &fake_modeller_, &fake_model_config_loader_,
        nullptr /* metrics_reporter */, task_environment_.GetMockTickClock());
    adapter_->Init();
    task_environment_.RunUntilIdle();
  }

  // Sets up all required input for Adapter and then creates Adapter.
  void Init(AlsReader::AlsInitStatus als_reader_status,
            BrightnessMonitor::Status brightness_monitor_status,
            const Model& model,
            const std::optional<ModelConfig>& model_config,
            const std::map<std::string, std::string>& params,
            bool brightness_set_by_policy = false) {
    fake_light_provider_->set_als_init_status(als_reader_status);
    fake_brightness_monitor_.set_status(brightness_monitor_status);
    fake_modeller_.InitModellerWithModel(model);
    if (model_config) {
      fake_model_config_loader_.set_model_config(model_config.value());
    }

    SetUpAdapter(params, brightness_set_by_policy);
  }

  void ReportSuspendDone() {
    chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
    task_environment_.RunUntilIdle();
  }

  void ReportLidEvent(chromeos::PowerManagerClient::LidState state) {
    chromeos::FakePowerManagerClient::Get()->SetLidState(
        state, task_environment_.NowTicks());
  }

  // Returns a valid ModelConfig.
  ModelConfig GetTestModelConfig(bool enabled = true) {
    ModelConfig model_config;
    model_config.auto_brightness_als_horizon_seconds = 5.0;
    model_config.enabled = enabled;
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

  void ReportAls(int als_value) {
    fake_light_provider_->ReportAmbientLightUpdate(als_value);
    task_environment_.RunUntilIdle();
  }

  void ReportUserBrightnessChangeRequest(double old_brightness_percent,
                                         double new_brightness_percent) {
    // Report a user-brightness-change-requested signal before a
    // user-brightness-changed signal to simulate the real brightness monitor.
    fake_brightness_monitor_.ReportUserBrightnessChangeRequested();
    fake_brightness_monitor_.ReportUserBrightnessChanged(
        old_brightness_percent, new_brightness_percent);
    task_environment_.RunUntilIdle();
  }

  // Forwards time first and then reports Als.
  void ForwardTimeAndReportAls(const std::vector<int>& als_values) {
    for (const int als_value : als_values) {
      // Forward 1 second to simulate the real AlsReader that samples data at
      // 1hz.
      task_environment_.FastForwardBy(base::Seconds(1));
      ReportAls(als_value);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  TestObserver test_observer_;

  std::unique_ptr<TestingProfile> profile_;

  std::optional<MonotoneCubicSpline> global_curve_;
  std::optional<MonotoneCubicSpline> personal_curve_;

  std::unique_ptr<FakeLightProvider> fake_light_provider_;
  std::unique_ptr<AlsReader> als_reader_;
  FakeBrightnessMonitor fake_brightness_monitor_;
  FakeModeller fake_modeller_;
  FakeModelConfigLoader fake_model_config_loader_;

  base::HistogramTester histogram_tester_;

  // |brightening_log_lux_threshold| and |darkening_log_lux_threshold| are set
  // to very small values so a slight change in ALS would trigger brightness
  // update. |stabilization_threshold| is set to a very high value so that we
  // don't have to check ALS has stablized.
  const std::map<std::string, std::string> default_params_ = {
      {"brightening_log_lux_threshold", "0.00001"},
      {"darkening_log_lux_threshold", "0.00001"},
      {"stabilization_threshold", "100000000"},
      {"user_adjustment_effect", "0"},
  };

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<Adapter> adapter_;
};

// AlsReader is |kDisabled| when Adapter is created.
TEST_F(AdapterTest, AlsReaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kDisabled, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// BrightnessMonitor is |kDisabled| when Adapter is created.
TEST_F(AdapterTest, BrightnessMonitorDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kDisabled,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// Modeller is |kDisabled| when Adapter is created.
TEST_F(AdapterTest, ModellerDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(), GetTestModelConfig(), default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// ModelConfigLoader has an invalid config, hence Modeller is disabled.
TEST_F(AdapterTest, ModelConfigLoaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), ModelConfig(), default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// AlsReader is |kDisabled| on later notification.
TEST_F(AdapterTest, AlsReaderDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_light_provider_->set_als_init_status(
      AlsReader::AlsInitStatus::kDisabled);
  fake_light_provider_->ReportReaderInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// AlsReader is |kSuccess| on later notification.
TEST_F(AdapterTest, AlsReaderEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_light_provider_->set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_light_provider_->ReportReaderInitialized();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// BrightnessMonitor is |kDisabled| on later notification.
TEST_F(AdapterTest, BrightnessMonitorDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kDisabled);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// BrightnessMonitor is |kSuccess| on later notification.
TEST_F(AdapterTest, BrightnessMonitorEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// Modeller is |kDisabled| on later notification.
TEST_F(AdapterTest, ModellerDisabledOnNotification) {
  fake_light_provider_->set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_model_config_loader_.set_model_config(GetTestModelConfig());
  SetUpAdapter(default_params_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_modeller_.InitModellerWithModel(Model());
  fake_modeller_.ReportModelInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
  EXPECT_FALSE(adapter_->GetGlobalCurveForTesting());
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// Modeller is |kSuccess| on later notification.
TEST_F(AdapterTest, ModellerEnabledOnNotification) {
  fake_light_provider_->set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_model_config_loader_.set_model_config(GetTestModelConfig());
  SetUpAdapter(default_params_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_modeller_.InitModellerWithModel(
      Model(global_curve_, personal_curve_, 0));
  fake_modeller_.ReportModelInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);
}

// ModelConfigLoader reports an invalid config on later notification.
TEST_F(AdapterTest, InvalidModelConfigOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), std::nullopt, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  // ModelConfig() creates an invalid config.
  DCHECK(!IsValidModelConfig(ModelConfig()));
  fake_model_config_loader_.set_model_config(ModelConfig());
  fake_model_config_loader_.ReportModelConfigLoaded();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// ModelConfigLoader reports a valid config on later notification.
TEST_F(AdapterTest, ValidModelConfigOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), std::nullopt, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_model_config_loader_.set_model_config(GetTestModelConfig());
  fake_model_config_loader_.ReportModelConfigLoaded();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// First ALS comes in 1 second after AlsReader is initialized. Hence after
// |auto_brightness_als_horizon_seconds|, brightness is changed.
TEST_F(AdapterTest, FirstAlsAfterAlsReaderInitTime) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  // |auto_brightness_als_horizon_seconds| is 5.
  ForwardTimeAndReportAls({1, 2, 3, 4});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

// First ALS comes in at the same time when AlsReader is initialized. Hence
// after |auto_brightness_als_horizon_seconds| + 1 readings, brightness is
// changed.
TEST_F(AdapterTest, FirstAlsAtAlsReaderInitTime) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  // First ALS when AlsReader is initialized.
  ReportAls(10);
  ForwardTimeAndReportAls({1, 2, 3, 4});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

TEST_F(AdapterTest, SequenceOfBrightnessUpdatesWithDefaultParams) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  ForwardTimeAndReportAls({1, 2, 3, 4});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  // Brightness is changed for the first time after the 5th reading.
  ForwardTimeAndReportAls({5});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Several other ALS readings come in, but need to wait for
  // |params.auto_brightness_als_horizon_seconds| to pass before having any
  // effect
  ForwardTimeAndReportAls({20});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({30});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({40});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({50});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // The next ALS reading triggers brightness change.
  ForwardTimeAndReportAls({60});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({20, 30, 40, 50, 60},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // |params.auto_brightness_als_horizon_seconds| has elapsed since we've made
  // the change, but there's no new ALS value, hence no brightness change is
  // triggered.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({20, 30, 40, 50, 60},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  EXPECT_EQ(adapter_->GetAverageAmbientWithStdDevForTesting(
                task_environment_.NowTicks()),
            std::nullopt);

  // A new ALS value triggers a brightness change.
  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 3);
  CheckAvgLog({100}, adapter_->GetCurrentAvgLogAlsForTesting().value());
}

// A user brightness change comes in when ALS readings exist. This also disables
// the adapter because |user_adjustment_effect| is 0 (disabled).
TEST_F(AdapterTest, UserBrightnessChangeAlsReadingExists) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  ForwardTimeAndReportAls({1, 2, 3, 4});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  // Adapter will not be applied after a user manual adjustment.
  ReportUserBrightnessChangeRequest(20.0, 30.0);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", false, 1);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());
  CheckAvgLog({1, 2, 3, 4}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // An als reading comes in but will not change the brightness.
  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  CheckAvgLog({1, 2, 3, 4}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another user manual adjustment comes in.
  task_environment_.FastForwardBy(base::Seconds(1));
  ReportUserBrightnessChangeRequest(30.0, 40.0);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", false, 2);
  CheckAvgLog({2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

// Same as |UserBrightnessChangeAlsReadingExists| except that user adjustment
// effect is Continue.
TEST_F(AdapterTest, UserBrightnessChangeAlsReadingExistsContinue) {
  std::map<std::string, std::string> params = default_params_;
  // UserAdjustmentEffect::kContinueAuto = 2.
  params["user_adjustment_effect"] = "2";
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  ForwardTimeAndReportAls({2, 4, 6, 8});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  // User brightness change comes in.
  ReportUserBrightnessChangeRequest(20.0, 30.0);
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", false, 1);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());
  EXPECT_EQ(test_observer_.num_changes(), 0);
  CheckAvgLog({2, 4, 6, 8}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Four ALS readings come in, but not enough time has passed since user
  // brightness change.
  ForwardTimeAndReportAls({4, 6, 8, 2});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  CheckAvgLog({2, 4, 6, 8}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another ALS reading is in but brightness isn't changed because there's no
  // new curve.
  ForwardTimeAndReportAls({5});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  CheckAvgLog({2, 4, 6, 8}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another model comes in.
  fake_modeller_.ReportModelTrained(*personal_curve_);
  EXPECT_EQ(test_observer_.num_changes(), 0);
  CheckAvgLog({2, 4, 6, 8}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another ALS reading is in and brightness is changed this time.
  ForwardTimeAndReportAls({15});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({6, 8, 2, 5, 15},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another user manual adjustment comes in.
  task_environment_.FastForwardBy(base::Seconds(1));
  ReportUserBrightnessChangeRequest(30.0, 40.0);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", false, 3);
  CheckAvgLog({8, 2, 5, 15}, adapter_->GetCurrentAvgLogAlsForTesting().value());
}

// Same as |UserBrightnessChangeAlsReadingExists| except that the 1st user
// brightness change comes when there is no ALS reading.
TEST_F(AdapterTest, UserBrightnessChangeAlsReadingAbsent) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  // Adapter will not be applied after a user manual adjustment.
  ReportUserBrightnessChangeRequest(20.0, 30.0);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", true, 1);
  EXPECT_EQ(adapter_->GetCurrentAvgLogAlsForTesting(), std::nullopt);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());
  EXPECT_FALSE(adapter_->GetCurrentAvgLogAlsForTesting());

  // ALS readings come in but will not change the brightness.
  ForwardTimeAndReportAls({100, 101, 102, 103, 104});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  EXPECT_FALSE(adapter_->GetCurrentAvgLogAlsForTesting());

  // Another user manual adjustment comes in.
  task_environment_.FastForwardBy(base::Seconds(1));
  ReportUserBrightnessChangeRequest(30.0, 40.0);
  histogram_tester_.ExpectBucketCount(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", true, 1);
  histogram_tester_.ExpectBucketCount(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", false, 1);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());
  CheckAvgLog({101, 102, 103, 104},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

// Same as |UserBrightnessChangeAlsReadingAbsent| except that user adjustment
// effect is Continue.
TEST_F(AdapterTest, UserBrightnessChangeAlsReadingAbsentContinue) {
  std::map<std::string, std::string> params = default_params_;
  // UserAdjustmentEffect::kContinueAuto = 2.
  params["user_adjustment_effect"] = "2";
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  ReportUserBrightnessChangeRequest(20.0, 30.0);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", true, 1);
  EXPECT_EQ(adapter_->GetCurrentAvgLogAlsForTesting(), std::nullopt);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());
  EXPECT_FALSE(adapter_->GetCurrentAvgLogAlsForTesting());

  // ALS readings come in, and will not trigger a brightness change because
  // there is no new model.
  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  ForwardTimeAndReportAls({101, 102, 103, 104});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  // Another user manual adjustment comes in.
  task_environment_.FastForwardBy(base::Seconds(1));
  ReportUserBrightnessChangeRequest(30.0, 40.0);
  histogram_tester_.ExpectBucketCount(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", true, 1);
  histogram_tester_.ExpectBucketCount(
      "AutoScreenBrightness.MissingAlsWhenBrightnessChanged", false, 1);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());
}

// Set |brightening_log_lux_threshold| to a very high value to effectively make
// brightening impossible.
TEST_F(AdapterTest, BrighteningThreshold) {
  std::map<std::string, std::string> params = default_params_;
  params["brightening_log_lux_threshold"] = "100";
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  ForwardTimeAndReportAls({1, 2, 3, 4});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  ForwardTimeAndReportAls({5});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 100);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 0.00001);

  ForwardTimeAndReportAls({4, 4, 4, 4, 4});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 100);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 0.00001);

  // Darkening is still possible.
  ForwardTimeAndReportAls({1});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 100);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 0.00001);

  ForwardTimeAndReportAls({1});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({4, 4, 4, 1, 1},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 100);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 0.00001);
}

// Set |darkening_log_lux_threshold| to a very high value to effectively make
// darkening impossible.
TEST_F(AdapterTest, DarkeningThreshold) {
  std::map<std::string, std::string> params = default_params_;
  params["darkening_log_lux_threshold"] = "100";
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  ForwardTimeAndReportAls({10, 20, 30, 40});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  ForwardTimeAndReportAls({50});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({10, 20, 30, 40, 50},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 0.00001);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 100);

  ForwardTimeAndReportAls({25, 25, 25, 25, 25});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({10, 20, 30, 40, 50},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 0.00001);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 100);

  ForwardTimeAndReportAls({40});
  CheckAvgLog({25, 25, 25, 25, 40},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() + 0.00001);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   adapter_->GetCurrentAvgLogAlsForTesting().value() - 100);
}

// Set |stabilization_threshold| to a very low value so that the average really
// should have little fluctuations before we change brightness.
TEST_F(AdapterTest, StablizationThreshold) {
  std::map<std::string, std::string> params = default_params_;
  params["stabilization_threshold"] = "0.00001";
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  ForwardTimeAndReportAls({10, 20, 30, 40, 50});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({10, 20, 30, 40, 50},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // A fluctuation means brightness is not changed.
  ForwardTimeAndReportAls({29, 29, 29, 29, 20});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({10, 20, 30, 40, 50},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({20, 20, 20, 20});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({20, 20, 20, 20, 20},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

// Shorten |auto_brightness_als_horizon| to 1 second. Averaging period is
// shorter and |stabilization_threshold| is ineffective in regularizing
// stabilization.
TEST_F(AdapterTest, AlsHorizon) {
  std::map<std::string, std::string> params = default_params_;
  // Small |stabilization_threshold|.
  params["stabilization_threshold"] = "0.00001";
  ModelConfig test_config = GetTestModelConfig();
  test_config.auto_brightness_als_horizon_seconds = 1;

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), test_config, params);

  ForwardTimeAndReportAls({10});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({10}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({100}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({2});
  EXPECT_EQ(test_observer_.num_changes(), 3);
  CheckAvgLog({2}, adapter_->GetCurrentAvgLogAlsForTesting().value());
}

TEST_F(AdapterTest, UseLatestCurve) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, std::nullopt, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  ForwardTimeAndReportAls({1, 2, 3, 4, 5});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Brightness is changed according to the global curve.
  EXPECT_DOUBLE_EQ(test_observer_.GetBrightnessPercent(),
                   global_curve_->Interpolate(
                       adapter_->GetCurrentAvgLogAlsForTesting().value()));

  // A new personal curve is received but adapter still uses the global curve.
  task_environment_.FastForwardBy(base::Seconds(20));
  fake_modeller_.ReportModelTrained(*personal_curve_);
  ReportAls(20);
  EXPECT_EQ(test_observer_.num_changes(), 2);
  EXPECT_EQ(test_observer_.GetCause(),
            power_manager::BacklightBrightnessChange_Cause_MODEL);

  // Brightness is changed according to the new personal curve.
  EXPECT_DOUBLE_EQ(test_observer_.GetBrightnessPercent(),
                   personal_curve_->Interpolate(
                       adapter_->GetCurrentAvgLogAlsForTesting().value()));
}

TEST_F(AdapterTest, BrightnessSetByPolicy) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_, true /* brightness_set_by_policy */);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  ForwardTimeAndReportAls({1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  EXPECT_EQ(adapter_->GetCurrentAvgLogAlsForTesting(), std::nullopt);
}

TEST_F(AdapterTest, FeatureDisabled) {
  // An empty param map will not enable the experiment.
  std::map<std::string, std::string> empty_params;

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       empty_params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);

  // Global and personal curves are received, but they won't be used to change
  // brightness.
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());

  // No brightness is changed.
  ForwardTimeAndReportAls({1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  EXPECT_EQ(adapter_->GetCurrentAvgLogAlsForTesting(), std::nullopt);
}

TEST_F(AdapterTest, FeatureEnabledConfigDisabled) {
  // Feature flag is enabled, but model config is disabled. Final effect is
  // disabled.
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0),
       GetTestModelConfig(false /* enabled */), default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);

  // Global and personal curves are received, but they won't be used to change
  // brightness.
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());

  // No brightness is changed.
  ForwardTimeAndReportAls({1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_EQ(test_observer_.num_changes(), 0);
  EXPECT_EQ(adapter_->GetCurrentAvgLogAlsForTesting(), std::nullopt);
}

TEST_F(AdapterTest, ValidParameters) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  histogram_tester_.ExpectTotalCount("AutoScreenBrightness.ParameterError", 0);
}

TEST_F(AdapterTest, InvalidParameters) {
  std::map<std::string, std::string> params = default_params_;
  params["user_adjustment_effect"] = "10";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.ParameterError",
      static_cast<int>(ParameterError::kAdapterError), 1);
}

TEST_F(AdapterTest, UserAdjustmentEffectDisable) {
  // |default_params_| sets the effect to disable.
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(),
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  // Brightness is changed for the 1st time.
  ForwardTimeAndReportAls({1, 2, 3, 4, 5});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Adapter will not be applied after a user manual adjustment.
  ReportUserBrightnessChangeRequest(20.0, 30.0);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());

  fake_modeller_.ReportModelTrained(*personal_curve_);
  ForwardTimeAndReportAls({6, 7, 8, 9, 10, 11});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // SuspendDone is received, which does not enable Adapter.
  ReportSuspendDone();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());

  ForwardTimeAndReportAls({11, 12, 13, 14, 15, 16});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

TEST_F(AdapterTest, UserAdjustmentEffectPause) {
  std::map<std::string, std::string> params = default_params_;
  // UserAdjustmentEffect::kPauseAuto = 1.
  params["user_adjustment_effect"] = "1";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  // Brightness is changed for the 1st time.
  ForwardTimeAndReportAls({1, 2, 3, 4, 5});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // User manually changes brightness so that adapter will not be applied.
  ReportUserBrightnessChangeRequest(20.0, 30.0);
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());

  // New ALS data will not trigger brightness update.
  ForwardTimeAndReportAls({101, 102, 103, 104, 105, 106, 107, 108});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // SuspendDone is received, which re-enables adapter.
  ReportSuspendDone();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());

  // Another ALS comes in but brightness isn't changed because there's no new
  // curve.
  ForwardTimeAndReportAls({109});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // A new model is received.
  fake_modeller_.ReportModelTrained(*personal_curve_);
  EXPECT_EQ(test_observer_.num_changes(), 1);
  ForwardTimeAndReportAls({110});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({106, 107, 108, 109, 110},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another user brightness change.
  ReportUserBrightnessChangeRequest(40.0, 50.0);
  CheckAvgLog({106, 107, 108, 109, 110},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_FALSE(adapter_->IsAppliedForTesting());

  // New ALS data will not trigger brightness update.
  ForwardTimeAndReportAls({200});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({106, 107, 108, 109, 110},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // SuspendDone is received, which reenables adapter.
  ReportSuspendDone();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());

  // A new model is in.
  fake_modeller_.ReportModelTrained(*personal_curve_);

  // Als readings come in but not sufficient time since user changed brightness.
  ForwardTimeAndReportAls({201, 202, 203});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({106, 107, 108, 109, 110},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({204});
  EXPECT_EQ(test_observer_.num_changes(), 3);
  CheckAvgLog({200, 201, 202, 203, 204},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

TEST_F(AdapterTest, UserAdjustmentEffectContinue) {
  std::map<std::string, std::string> params = default_params_;
  // UserAdjustmentEffect::kContinueAuto = 2.
  params["user_adjustment_effect"] = "2";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  // Brightness is changed for the 1st time.
  ForwardTimeAndReportAls({1, 2, 3, 4, 5});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 5},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({10});
  // User manual adjustment doesn't disable adapter.
  ReportUserBrightnessChangeRequest(40.0, 50.0);
  CheckAvgLog({2, 3, 4, 5, 10},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->IsAppliedForTesting());

  ForwardTimeAndReportAls({100, 101, 102, 103});
  CheckAvgLog({2, 3, 4, 5, 10},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({104});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({2, 3, 4, 5, 10},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

TEST_F(AdapterTest, LidEvents) {
  std::map<std::string, std::string> params = default_params_;
  params["lid_open_delay_time_seconds"] = "3";
  ModelConfig test_config = GetTestModelConfig();
  test_config.auto_brightness_als_horizon_seconds = 3;

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), test_config, params);

  // |auto_brightness_als_horizon_seconds| is 3.
  ForwardTimeAndReportAls({1, 2});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 100}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ReportLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);

  // All ALS values that arrive after lid is closed are ignored.
  ForwardTimeAndReportAls({0});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 100}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({200});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 100}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);

  // ALS readings that arrive in the next 2 seconds will be ignored because
  // |lid_open_delay_time_seconds| is set to 3 seconds.
  ForwardTimeAndReportAls({300});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 100}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({400});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 100}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Another ALS reading arrives 2 seconds after lid-open. Brightness is changed
  // immediately. As earlier ALS readings were cleared when lid was closed, only
  // one ALS reading is used to calculate brightness.
  ForwardTimeAndReportAls({500});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({500}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Next two ALS readings won't change brightness because we are waiting for
  // averaging period |auto_brightness_als_horizon_seconds| to pass.
  ForwardTimeAndReportAls({600});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({500}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({700});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({500}, adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Averaging period has passed, so brightness is changed.
  ForwardTimeAndReportAls({800});
  EXPECT_EQ(test_observer_.num_changes(), 3);
  CheckAvgLog({600, 700, 800},
              adapter_->GetCurrentAvgLogAlsForTesting().value());
}

TEST_F(AdapterTest, SuspendDueToLidClosed) {
  std::map<std::string, std::string> params = default_params_;
  // UserAdjustmentEffect::kPauseAuto = 1.
  params["user_adjustment_effect"] = "1";
  params["lid_open_delay_time_seconds"] = "2";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       Model(global_curve_, personal_curve_, 0), GetTestModelConfig(), params);

  // |auto_brightness_als_horizon_seconds| is 5.
  ForwardTimeAndReportAls({1, 2, 3, 4});
  EXPECT_EQ(test_observer_.num_changes(), 0);

  ForwardTimeAndReportAls({100});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  // Lid is closed and triggers a suspend (no need to report suspend here).
  ReportLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  ForwardTimeAndReportAls({0});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({200});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  ReportSuspendDone();

  // First ALS reading that arrives after lid-open will be ignored because
  // |lid_open_delay_time_seconds| is set to 2 seconds.
  ForwardTimeAndReportAls({300});
  EXPECT_EQ(test_observer_.num_changes(), 1);
  CheckAvgLog({1, 2, 3, 4, 100},
              adapter_->GetCurrentAvgLogAlsForTesting().value());

  ForwardTimeAndReportAls({400});
  EXPECT_EQ(test_observer_.num_changes(), 2);
  CheckAvgLog({400}, adapter_->GetCurrentAvgLogAlsForTesting().value());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
