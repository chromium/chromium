// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
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
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#endif

// ChromeBrowserMainExtraParts implementation that asserts the metrics and
// reporting state matches a particular value in PreCreateThreads().
class ChromeBrowserMainExtraPartsChecker : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserMainExtraPartsChecker(
      bool expected_metrics_reporting_enabled)
      : expected_metrics_reporting_enabled_(
            expected_metrics_reporting_enabled) {}

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;

 private:
  // Expected value of reporting state.
  const bool expected_metrics_reporting_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsChecker);
};

// This class is used to verify the value for
// IsMetricsAndCrashReportingEnabled() is honored from prefs and when changing
// the value it is correctly written to disk. The parameter of this test
// corresponds to the initial value of IsMetricsAndCrashReportingEnabled().
class MetricsReportingStateTest : public InProcessBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  MetricsReportingStateTest() = default;
  ~MetricsReportingStateTest() override = default;

  static bool IsMetricsAndCrashReportingEnabled() {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }

  bool is_metrics_reporting_enabled_initial_value() const { return GetParam(); }

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
    bool enabled = false;
    ASSERT_TRUE(pref_dict_values->GetBoolean(
        metrics::prefs::kMetricsReportingEnabled, &enabled));
    EXPECT_EQ(!is_metrics_reporting_enabled_initial_value(), enabled);
    InProcessBrowserTest::TearDown();
  }

 private:
  base::FilePath local_state_path_;

  DISALLOW_COPY_AND_ASSIGN(MetricsReportingStateTest);
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

IN_PROC_BROWSER_TEST_P(MetricsReportingStateTest, ChangeMetricsReportingState) {
  ASSERT_EQ(is_metrics_reporting_enabled_initial_value(),
            MetricsReportingStateTest::IsMetricsAndCrashReportingEnabled());
  base::RunLoop run_loop;
  bool value_after_change = false;
  ChangeMetricsReportingStateWithReply(
      !is_metrics_reporting_enabled_initial_value(),
      base::BindOnce(&OnMetricsReportingStateChanged, &value_after_change,
                     run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(!is_metrics_reporting_enabled_initial_value(), value_after_change);
}

INSTANTIATE_TEST_SUITE_P(MetricsReportingStateTests,
                         MetricsReportingStateTest,
                         testing::Bool());
