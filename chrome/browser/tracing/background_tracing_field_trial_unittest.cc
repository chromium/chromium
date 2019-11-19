// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_command_line.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class BackgroundTracingTest : public testing::Test {
 public:
  BackgroundTracingTest() = default;

  void TearDown() override {
    content::BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

namespace {

const char kTestConfig[] = "test";
bool g_test_config_loaded = false;

const char kUploadUrl[] = "http://localhost:8080";
const char kInvalidTracingConfig[] = "{][}";
const char kValidTracingConfig[] = R"(
  {
    "scenario_name": "BrowserProcess",
    "configs": [
      {
        "category": "BENCHMARK_NAVIGATION",
        "rule": "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE",
        "histogram_name": "Omnibox.CharTypedToRepaintLatency.ToPaint",
        "histogram_lower_value": 1
      }
    ],
    "mode": "REACTIVE_TRACING_MODE"
  }
)";

void CheckConfig(std::string* config) {
  if (*config == kTestConfig)
    g_test_config_loaded = true;
}

}  // namespace

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFieldTrial) {
  const std::string kTrialName = "BackgroundTracing";
  const std::string kExperimentName = "SlowStart";
  base::AssociateFieldTrialParams(kTrialName, kExperimentName,
                                  {{"config", kTestConfig}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kExperimentName);

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  // In case it is already set at previous test run.
  g_test_config_loaded = false;

  tracing::SetConfigTextFilterForTesting(&CheckConfig);

  tracing::SetupBackgroundTracingFieldTrial();
  EXPECT_TRUE(g_test_config_loaded);
}

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFromConfigFileFailed) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kTraceUploadURL, kUploadUrl);
  command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing, "");

  tracing::SetupBackgroundTracingFieldTrial();
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
}

TEST_F(BackgroundTracingTest,
       SetupBackgroundTracingFromConfigFileInvalidConfig) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(file_path, kInvalidTracingConfig,
                  sizeof(kInvalidTracingConfig) - 1);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kTraceUploadURL, kUploadUrl);
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing, file_path);

  tracing::SetupBackgroundTracingFieldTrial();
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
}

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFromConfigFile) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(file_path, kValidTracingConfig,
                  sizeof(kValidTracingConfig) - 1);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kTraceUploadURL, kUploadUrl);
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing, file_path);

  tracing::SetupBackgroundTracingFieldTrial();
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
}
