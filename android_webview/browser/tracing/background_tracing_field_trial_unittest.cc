// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/background_tracing_field_trial.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_proto_loader.h"
#include "base/threading/thread_restrictions.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kTrialName = "BackgroundWebviewTracing";
const std::string kGroupName = "BackgroundWebviewTracing1";
const char kTestConfigReactiveMode[] = R"(
  {
    "scenario_name": "BackgroundTracing",
    "configs": [
      {
        "rule": "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE",
        "histogram_name": "Android.WebView.LoadUrl.UrlScheme",
        "histogram_lower_value": 0
      }
    ],
    "mode": "REACTIVE_TRACING_MODE"
  }
)";

const char kTestConfigPreemptiveMode[] = R"(
  {
    "scenario_name": "BackgroundTracing",
    "custom_categories": "toplevel",
    "configs": [
      {
        "rule": "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE",
        "histogram_name": "Android.WebView.LoadUrl.UrlScheme",
        "histogram_lower_value": 0
      }
    ],
    "mode": "PREEMPTIVE_TRACING_MODE"
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

class BackgroundTracingTest : public testing::Test {
 public:
  BackgroundTracingTest() {
    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_client_);
    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance();
  }

  void TearDown() override {
    background_tracing_manager_.reset();
    content::SetBrowserClientForTesting(nullptr);
    content::SetContentClient(nullptr);
    base::FieldTrialParamAssociator::GetInstance()->ClearParamsForTesting(
        kTrialName, kGroupName);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::ContentClient content_client_;
  content::ContentBrowserClient browser_client_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

}  // namespace

TEST_F(BackgroundTracingTest, ReactiveConfigSystemSetup) {
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, {{"config", kTestConfigReactiveMode}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  EXPECT_FALSE(android_webview::MaybeSetupSystemTracingFromFieldTrial());

  // Config (reactive) and method call (system) mismatch, nothing should be set
  // up.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, ReactiveConfigWebViewOnlySetup) {
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, {{"config", kTestConfigReactiveMode}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  EXPECT_TRUE(android_webview::MaybeSetupWebViewOnlyTracingFromFieldTrial());

  // Config (reactive) and method call (webview-only) match.
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, PreemptiveConfigSystemSetup) {
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, {{"config", kTestConfigPreemptiveMode}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  EXPECT_FALSE(android_webview::MaybeSetupSystemTracingFromFieldTrial());

  // Config (preemptive) and method call (system) mismatch, nothing should be
  // set up.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, SetupBackgroundTracingFromProtoConfigFile) {
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
            tracing::BackgroundTracingSetupMode::kFromProtoConfigFile);
  EXPECT_FALSE(android_webview::MaybeSetupSystemTracingFromFieldTrial());
  EXPECT_FALSE(android_webview::MaybeSetupWebViewOnlyTracingFromFieldTrial());
}
