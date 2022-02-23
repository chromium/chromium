// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#endif

using ::testing::Optional;

struct MetricsReportingStateTestParameterizedParams {
  bool initial_value;
  bool final_value;
};

// ChromeBrowserMainExtraParts implementation that asserts the metrics and
// reporting state matches a particular value in PreCreateThreads().
class ChromeBrowserMainExtraPartsChecker : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserMainExtraPartsChecker(
      bool expected_metrics_reporting_enabled)
      : expected_metrics_reporting_enabled_(
            expected_metrics_reporting_enabled) {}

  ChromeBrowserMainExtraPartsChecker(
      const ChromeBrowserMainExtraPartsChecker&) = delete;
  ChromeBrowserMainExtraPartsChecker& operator=(
      const ChromeBrowserMainExtraPartsChecker&) = delete;

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;

 private:
  // Expected value of reporting state.
  const bool expected_metrics_reporting_enabled_;
};

// Used to appropriately set up the initial value of
// IsMetricsAndCrashReportingEnabled().
class MetricsReportingStateTest : public InProcessBrowserTest {
 public:
  MetricsReportingStateTest(const MetricsReportingStateTest&) = delete;
  MetricsReportingStateTest& operator=(const MetricsReportingStateTest&) =
      delete;

  ~MetricsReportingStateTest() override = default;

  static bool IsMetricsAndCrashReportingEnabled() {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }

  virtual bool is_metrics_reporting_enabled_initial_value() const = 0;

  // InProcessBrowserTest overrides:
  bool SetUpUserDataDirectory() override {
    local_state_path_ = metrics::SetUpUserDataDirectoryForTesting(
        is_metrics_reporting_enabled_initial_value());
    return !local_state_path_.empty();
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    // IsMetricsReportingEnabled() in non-official builds always returns false.
    // Enable the official build checks so that this test can work in both
    // official and non-official builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<ChromeBrowserMainExtraPartsChecker>(
            is_metrics_reporting_enabled_initial_value()));
  }

 protected:
  MetricsReportingStateTest() = default;

  base::FilePath local_state_path_;
};

// Used to verify the value for IsMetricsAndCrashReportingEnabled() is correctly
// written to disk when changed. The first parameter of this test corresponds
// to the initial value of IsMetricsAndCrashReportingEnabled(). The second
// parameter corresponds to what the value should change to during the test.
class MetricsReportingStateTestParameterized
    : public MetricsReportingStateTest,
      public testing::WithParamInterface<
          MetricsReportingStateTestParameterizedParams> {
 public:
  MetricsReportingStateTestParameterized() = default;

  MetricsReportingStateTestParameterized(
      const MetricsReportingStateTestParameterized&) = delete;
  MetricsReportingStateTestParameterized& operator=(
      const MetricsReportingStateTestParameterized&) = delete;

  ~MetricsReportingStateTestParameterized() override = default;

  bool is_metrics_reporting_enabled_initial_value() const override {
    return GetParam().initial_value;
  }

  bool is_metrics_reporting_enabled_final_value() const {
    return GetParam().final_value;
  }

  void TearDown() override {
    // Verify the changed value was written to disk.
    JSONFileValueDeserializer deserializer(local_state_path_);
    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> pref_values =
        deserializer.Deserialize(&error_code, &error_message);
    ASSERT_TRUE(pref_values) << error_message;
    base::DictionaryValue* pref_dict_values = nullptr;
    ASSERT_TRUE(pref_values->GetAsDictionary(&pref_dict_values));
    EXPECT_THAT(pref_dict_values->FindBoolPath(
                    metrics::prefs::kMetricsReportingEnabled),
                Optional(is_metrics_reporting_enabled_final_value()));
    InProcessBrowserTest::TearDown();
  }
};

// Used to verify that metrics collected during a session are discarded upon
// enabling metrics reporting, so that only data collected after enabling
// metrics are collected. Histogram data should only be cleared if metrics
// reporting was enabled from a settings page.
class MetricsReportingStateClearDataTest
    : public MetricsReportingStateTest,
      public testing::WithParamInterface<
          ChangeMetricsReportingStateCalledFrom> {
 public:
  // Set metrics reporting to false initially.
  bool is_metrics_reporting_enabled_initial_value() const override {
    return false;
  }
};

void ChromeBrowserMainExtraPartsChecker::PostEarlyInitialization() {
  ASSERT_EQ(expected_metrics_reporting_enabled_,
            MetricsReportingStateTest::IsMetricsAndCrashReportingEnabled());
}

// Callback from changing whether reporting is enabled.
void OnMetricsReportingStateChanged(bool* new_state_ptr,
                                    base::OnceClosure run_loop_closure,
                                    bool new_state) {
  *new_state_ptr = new_state;
  std::move(run_loop_closure).Run();
}

bool HistogramExists(base::StringPiece name) {
  return base::StatisticsRecorder::FindHistogram(name) != nullptr;
}

base::HistogramBase::Count GetHistogramDeltaTotalCount(base::StringPiece name) {
  return base::StatisticsRecorder::FindHistogram(name)
      ->SnapshotDelta()
      ->TotalCount();
}

// Verifies that metrics reporting state is correctly written to disk when set.
// See also MetricsReportingStateTestParameterized::TearDown.
IN_PROC_BROWSER_TEST_P(MetricsReportingStateTestParameterized,
                       ChangeMetricsReportingState) {
  ASSERT_EQ(is_metrics_reporting_enabled_initial_value(),
            MetricsReportingStateTest::IsMetricsAndCrashReportingEnabled());
  base::RunLoop run_loop;
  bool value_after_change = false;
  ChangeMetricsReportingStateWithReply(
      is_metrics_reporting_enabled_final_value(),
      base::BindOnce(&OnMetricsReportingStateChanged, &value_after_change,
                     run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(is_metrics_reporting_enabled_final_value(), value_after_change);
}

// Verifies that collected data is cleared after enabling metrics reporting.
// Histogram data should only be cleared (marked as reported) when enabling
// metrics reporting from a settings page.
IN_PROC_BROWSER_TEST_P(MetricsReportingStateClearDataTest,
                       ClearPreviouslyCollectedMetricsData) {
  // Set Stablity Crash Count metric to 1.
  g_browser_process->local_state()->SetInteger(
      metrics::prefs::kStabilityCrashCount, 1);
  // Emit to two histograms.
  ASSERT_FALSE(HistogramExists("Test.Before.Histogram"));
  ASSERT_FALSE(HistogramExists("Test.Before.StabilityHistogram"));
  base::UmaHistogramBoolean("Test.Before.Histogram", true);
  UMA_STABILITY_HISTOGRAM_BOOLEAN("Test.Before.StabilityHistogram", true);
  ASSERT_TRUE(HistogramExists("Test.Before.Histogram"));
  ASSERT_TRUE(HistogramExists("Test.Before.StabilityHistogram"));

  // Simulate enabling metrics reporting.
  ChangeMetricsReportingStateCalledFrom called_from = GetParam();
  base::RunLoop run_loop;
  bool value_after_change = false;
  ChangeMetricsReportingStateWithReply(
      true,
      base::BindOnce(&OnMetricsReportingStateChanged, &value_after_change,
                     run_loop.QuitClosure()),
      called_from);
  run_loop.Run();
  ASSERT_TRUE(value_after_change);

  // Emit to one histogram after enabling metrics reporting.
  ASSERT_FALSE(HistogramExists("Test.After.Histogram"));
  base::UmaHistogramBoolean("Test.After.Histogram", true);
  ASSERT_TRUE(HistogramExists("Test.After.Histogram"));

  // Verify that stability metrics were cleared.
  EXPECT_EQ(0, g_browser_process->local_state()->GetInteger(
                   metrics::prefs::kStabilityCrashCount));
  // Verify that histogram data that came before clearing data are not included
  // in the next snapshot if metrics reporting was enabled from a settings page.
  bool called_from_settings_page =
      (called_from == ChangeMetricsReportingStateCalledFrom::kUiSettings);
  EXPECT_EQ(called_from_settings_page ? 0 : 1,
            GetHistogramDeltaTotalCount("Test.Before.Histogram"));
  EXPECT_EQ(called_from_settings_page ? 0 : 1,
            GetHistogramDeltaTotalCount("Test.Before.StabilityHistogram"));
  // Verify that histogram data that came after clearing data is included in the
  // next snapshot.
  EXPECT_EQ(1, GetHistogramDeltaTotalCount("Test.After.Histogram"));

  // Clean up histograms.
  base::StatisticsRecorder::ForgetHistogramForTesting("Test.Before.Histogram");
  base::StatisticsRecorder::ForgetHistogramForTesting(
      "Test.Before.StabilityHistogram");
  base::StatisticsRecorder::ForgetHistogramForTesting("Test.After.Histogram");
}

INSTANTIATE_TEST_SUITE_P(
    MetricsReportingStateTests,
    MetricsReportingStateTestParameterized,
    testing::ValuesIn<MetricsReportingStateTestParameterizedParams>(
        // The first param determines what is the initial state of metrics
        // reporting at the beginning of the test. The second param determines
        // what the metrics reporting state should change to during the test.
        {{false, false}, {false, true}, {true, false}, {true, true}}));

INSTANTIATE_TEST_SUITE_P(
    MetricsReportingStateTests,
    MetricsReportingStateClearDataTest,
    testing::ValuesIn<ChangeMetricsReportingStateCalledFrom>(
        {ChangeMetricsReportingStateCalledFrom::kUnknown,
         ChangeMetricsReportingStateCalledFrom::kUiSettings}));
