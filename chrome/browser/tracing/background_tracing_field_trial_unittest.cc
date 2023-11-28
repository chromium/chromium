// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_proto_loader.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class BackgroundTracingTest : public testing::Test {
 public:
  BackgroundTracingTest() {
    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance();
  }

  void TearDown() override {
    tracing::BackgroundTracingStateManager::GetInstance().ResetForTesting();
    content::BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  }

 protected:
  // Needs to stay alive through TearDown().
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

namespace {

const char kValidJsonTracingConfig[] = R"(
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

const char kValidProtoTracingConfig[] = R"pb(
  scenarios: {
    scenario_name: "test_scenario"
    start_rules: { name: "start_trigger" manual_trigger_name: "start_trigger" }
    upload_rules: {
      name: "upload_trigger"
      manual_trigger_name: "upload_trigger"
    }
    trace_config: {
      data_sources: { config: { name: "org.chromium.trace_metadata" } }
    }
  }
)pb";

using tracing::BackgroundTracingSetupMode;

std::string GetFieldTracingConfigFromText(const std::string& proto_text) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.ChromeFieldTracingConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  return serialized_message;
}

}  // namespace

TEST_F(BackgroundTracingTest, MaybeSetupBackgroundTracingFromFieldTrial) {
  const std::string kTrialName = "BackgroundTracing";
  const std::string kExperimentName = "SlowStart";
  base::AssociateFieldTrialParams(kTrialName, kExperimentName,
                                  {{"config", kValidJsonTracingConfig}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kExperimentName);

  testing_profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager_->SetUp());

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_FALSE(tracing::MaybeSetupSystemTracingFromFieldTrial());
  EXPECT_TRUE(tracing::MaybeSetupBackgroundTracingFromFieldTrial());
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, MaybeSetupFieldTracingFromFieldTrial) {
  std::string serialized_config =
      GetFieldTracingConfigFromText(kValidProtoTracingConfig);
  std::string encoded_config;
  base::Base64Encode(serialized_config, &encoded_config);
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(tracing::kFieldTracing,
                                                 {{"config", encoded_config}});

  testing_profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager_->SetUp());

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_FALSE(tracing::MaybeSetupSystemTracingFromFieldTrial());
  EXPECT_TRUE(tracing::MaybeSetupBackgroundTracingFromFieldTrial());
}

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFromProtoConfigFile) {
  testing_profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager_->SetUp());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("config.pb");
  base::WriteFile(file_path,
                  GetFieldTracingConfigFromText(kValidProtoTracingConfig));

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(
      switches::kBackgroundTracingOutputFile,
      temp_dir.GetPath().AppendASCII("test_trace.perfetto.gz"));
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing, file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromProtoConfigFile);
  EXPECT_FALSE(tracing::MaybeSetupSystemTracingFromFieldTrial());
  EXPECT_FALSE(tracing::MaybeSetupBackgroundTracingFromFieldTrial());
  EXPECT_TRUE(tracing::SetupBackgroundTracingFromCommandLine());
}

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFromJsonConfigFile) {
  testing_profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager_->SetUp());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(file_path, kValidJsonTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(
      switches::kBackgroundTracingOutputFile,
      temp_dir.GetPath().AppendASCII("test_trace.perfetto.gz"));
  command_line->AppendSwitchPath(switches::kEnableLegacyBackgroundTracing,
                                 file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromJsonConfigFile);
  EXPECT_FALSE(tracing::MaybeSetupBackgroundTracingFromFieldTrial());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFieldTrialOutputFile) {
  const std::string kTrialName = "BackgroundTracing";
  const std::string kExperimentName = "LocalOutput";
  base::AssociateFieldTrialParams(kTrialName, kExperimentName,
                                  {{"config", kValidJsonTracingConfig}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kExperimentName);

  testing_profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager_->SetUp());

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(
      switches::kBackgroundTracingOutputFile,
      temp_dir.GetPath().AppendASCII("test_trace.perfetto.gz"));

  ASSERT_TRUE(tracing::HasBackgroundTracingOutputFile());
  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_TRUE(tracing::MaybeSetupBackgroundTracingFromFieldTrial());

  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

}  // namespace tracing
