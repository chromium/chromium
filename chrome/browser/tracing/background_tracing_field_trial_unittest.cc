// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/tracing/public/cpp/tracing_features.h"
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

const char kInvalidTracingConfig[] = "{][}";
const char kValidTracingConfig[] = R"(
  {
    "scenario_name": "BrowserProcess",
    "configs": [
      {
        "custom_categories": "base,toplevel",
        "rule": "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE",
        "histogram_name": "Omnibox.CharTypedToRepaintLatency.ToPaint",
        "histogram_lower_value": 1
      }
    ],
    "mode": "REACTIVE_TRACING_MODE"
  }
)";

std::string CheckConfig(const std::string& config) {
  if (config == kTestConfig)
    g_test_config_loaded = true;
  return config;
}

using tracing::BackgroundTracingSetupMode;

struct SetupModeParams {
  const char* enable_background_tracing = nullptr;
  const char* trace_output_file = nullptr;
  BackgroundTracingSetupMode expected_mode;
};

}  // namespace

TEST_F(BackgroundTracingTest, GetBackgroundTracingSetupMode) {
  const std::vector<SetupModeParams> kParams = {
      // No config file param.
      {nullptr, nullptr, BackgroundTracingSetupMode::kFromFieldTrial},
      // Empty config filename.
      {"", "output_file.gz",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // No output location switch.
      {"config.json", nullptr,
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // Empty output location switch.
      {"config.json", "",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // file is valid for proto traces.
      {"config.json", "output_file.gz",
       BackgroundTracingSetupMode::kFromConfigFile},
  };

  for (const SetupModeParams& params : kParams) {
    SCOPED_TRACE(::testing::Message()
                 << "enable_background_tracing "
                 << params.enable_background_tracing << " trace_output_file "
                 << params.trace_output_file);
    base::test::ScopedFeatureList feature_list;
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    if (params.enable_background_tracing) {
      command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing,
                                      params.enable_background_tracing);
    }
    if (params.trace_output_file) {
      command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile,
                                      params.trace_output_file);
    }

    EXPECT_EQ(tracing::GetBackgroundTracingSetupMode(), params.expected_mode);
  }
  }

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

  content::BackgroundTracingManager::GetInstance()
      ->SetConfigTextFilterForTesting(base::BindRepeating(&CheckConfig));

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
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
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");
  command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
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
  command_line->AppendSwitchPath(
      switches::kBackgroundTracingOutputFile,
      temp_dir.GetPath().AppendASCII("test_trace.perfetto.gz"));
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing, file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromConfigFile);
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
  command_line->AppendSwitchPath(
      switches::kBackgroundTracingOutputFile,
      temp_dir.GetPath().AppendASCII("test_trace.perfetto.gz"));
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing, file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromConfigFile);
  tracing::SetupBackgroundTracingFieldTrial();
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
}
