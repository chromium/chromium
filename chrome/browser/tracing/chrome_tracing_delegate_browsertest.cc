// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/named_trigger.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/pref_names.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/background_tracing_test_support.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace {

class TestBackgroundTracingHelper
    : public content::BackgroundTracingManager::EnabledStateTestObserver {
 public:
  TestBackgroundTracingHelper() {
    content::AddBackgroundTracingEnabledStateObserverForTesting(this);
  }

  ~TestBackgroundTracingHelper() {
    content::RemoveBackgroundTracingEnabledStateObserverForTesting(this);
  }

  void OnScenarioIdle(const std::string& scenario_name) override {
    wait_for_scenario_idle_.Quit();
  }

  void OnTraceReceived(const std::string& proto_content) override {
    wait_for_trace_received_.Quit();
  }

  void WaitForScenarioIdle() { wait_for_scenario_idle_.Run(); }
  void WaitForTraceReceived() { wait_for_trace_received_.Run(); }

 private:
  base::RunLoop wait_for_scenario_idle_;
  base::RunLoop wait_for_trace_received_;
};

}  // namespace

namespace tracing {

class ChromeTracingDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ChromeTracingDelegateBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    content::TracingController::GetInstance();  // Create tracing agents.
  }

  bool StartPreemptiveScenario(
      content::BackgroundTracingManager::DataFiltering data_filtering,
      std::string_view scenario_name = "TestScenario",
      bool with_crash_scenario = false) {
    base::Value::Dict dict;

    dict.Set("scenario_name", scenario_name);
    dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
    dict.Set("custom_categories", "toplevel");

    base::Value::List rules_list;
    {
      base::Value::Dict rules_dict;
      rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict.Set("trigger_name", "test");
      rules_list.Append(std::move(rules_dict));
    }
    if (with_crash_scenario) {
      base::Value::Dict rules_dict;
      rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict.Set("trigger_name", "test_crash");
      rules_dict.Set("is_crash", true);
      rules_list.Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(std::move(dict)));
    DCHECK(config);

    return content::BackgroundTracingManager::GetInstance().SetActiveScenario(
        std::move(config), data_filtering);
  }

  bool StartPreemptiveScenarioWithCrash(
      content::BackgroundTracingManager::DataFiltering data_filtering,
      const std::string& scenario_name = "TestScenario") {
    return StartPreemptiveScenario(data_filtering, scenario_name,
                                   /*with_crash_scenario=*/true);
  }

  void TriggerPreemptiveScenario(const std::string& trigger_name = "test") {
    base::trace_event::EmitNamedTrigger(trigger_name);
  }

  void TriggerPreemptiveScenarioWithCrash() {
    TriggerPreemptiveScenario("test_crash");
  }
};

std::string GetSessionStateJson() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Value::Dict& state =
      local_state->GetDict(tracing::kBackgroundTracingSessionState);
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(state, &json));
  return json;
}

void SetSessionState(base::Value::Dict dict) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->Set(tracing::kBackgroundTracingSessionState,
                   base::Value(std::move(dict)));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingUnexpectedSessionEnd) {
  base::Value::Dict dict;
  dict.Set("state", static_cast<int>(tracing::BackgroundTracingState::STARTED));
  SetSessionState(std::move(dict));
  tracing::BackgroundTracingStateManager::GetInstance().ResetForTesting();

  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingSessionRanLong) {
  base::Value::Dict dict;
  dict.Set("state",
           static_cast<int>(tracing::BackgroundTracingState::RAN_30_SECONDS));
  SetSessionState(std::move(dict));
  tracing::BackgroundTracingStateManager::GetInstance().ResetForTesting();

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingFinalizationStarted) {
  base::Value::Dict dict;
  dict.Set("state", static_cast<int>(
                        tracing::BackgroundTracingState::FINALIZATION_STARTED));
  SetSessionState(std::move(dict));
  tracing::BackgroundTracingStateManager::GetInstance().ResetForTesting();

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

// If we need a PII-stripped trace, any existing OTR session should block the
// trace.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       ExistingIncognitoSessionBlockingTraceStart) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());
  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::ANONYMIZE_DATA));
}

// If we need a PII-stripped trace, OTR sessions that ended before tracing
// should block the trace (because traces could theoretically include stale
// memory from those sessions).
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       FinishedIncognitoSessionBlockingTraceStart) {
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());
  CloseBrowserSynchronously(incognito_browser);
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserActive());
}

// If we need a PII-stripped trace, any new OTR session during tracing should
// block the finalization of the trace.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       NewIncognitoSessionBlockingTraceFinalization) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::ANONYMIZE_DATA));

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());

  TestBackgroundTracingHelper background_tracing_helper;
  TriggerPreemptiveScenario();
  background_tracing_helper.WaitForScenarioIdle();
}

// If we need a PII-stripped trace, any OTR session that starts and ends during
// tracing should block the finalization of the trace.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       ShortIncognitoSessionBlockingTraceFinalization) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::ANONYMIZE_DATA));

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());
  CloseBrowserSynchronously(incognito_browser);
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserActive());

  TestBackgroundTracingHelper background_tracing_helper;
  TriggerPreemptiveScenario();
  background_tracing_helper.WaitForScenarioIdle();
}

namespace {
static const char* const kDefaultConfigText = R"({
        "mode": "PREEMPTIVE_TRACING_MODE",
        "scenario_name": "TestScenario",
        "custom_categories": "base,toplevel",
        "configs": [{"rule": "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED",
                     "trigger_name": "test"}]
        })";
}  // namespace

class ChromeTracingDelegateBrowserTestOnStartup
    : public ChromeTracingDelegateBrowserTest {
 protected:
  ChromeTracingDelegateBrowserTestOnStartup() {
    variations::testing::VariationParamsManager::SetVariationParams(
        "BackgroundTracing", "TestGroup", {{"config", kDefaultConfigText}});
  }
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_ScenarioSetFromFieldtrial) {
  // This test would enable tracing and shutdown browser before 30 seconds
  // elapses. So, the profile would store incomplete state for next session.
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 1 = STARTED.
  EXPECT_EQ(GetSessionStateJson(), R"({"privacy_filter":true,"state":1})");
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       ScenarioSetFromFieldtrial) {
  // Scenario should be inactive even though we have a config because last
  // session shut down unexpectedly.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 0 = NOT_ACTIVATED, current session is inactive.
  EXPECT_EQ(GetSessionStateJson(), R"({"privacy_filter":true,"state":0})");
}

class ChromeTracingDelegateBrowserTestFromCommandLine
    : public ChromeTracingDelegateBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeTracingDelegateBrowserTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath config_path(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("config.json")));
    ASSERT_TRUE(base::WriteFile(config_path, kDefaultConfigText));
    command_line->AppendSwitchPath("enable-legacy-background-tracing",
                                   config_path);

    output_path_ = temp_dir_.GetPath();
    command_line->AppendSwitchPath("background-tracing-output-path",
                                   output_path_);
  }

  bool OutputFileExists() const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FileEnumerator e(output_path_, false, base::FileEnumerator::FILES,
                           FILE_PATH_LITERAL("*.perfetto.gz"));
    for (base::FilePath name = e.Next(); !name.empty();) {
      return true;
    }
    return false;
  }

  void TriggerScenarioAndWaitForOutput() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Wait for the output file to appear instead of for the trigger callback
    // (which just means the data is ready to write).
    base::FilePathWatcher output_watcher;
    base::RunLoop run_loop;
    output_watcher.Watch(output_path_, base::FilePathWatcher::Type::kRecursive,
                         base::BindLambdaForTesting(
                             [&run_loop, this](const base::FilePath&, bool) {
                               if (OutputFileExists()) {
                                 run_loop.Quit();
                               }
                             }));
    TriggerPreemptiveScenario();
    run_loop.Run();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath output_path_;
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestFromCommandLine,
                       ScenarioFromCommandLine) {
  ASSERT_FALSE(OutputFileExists());

  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 1 = STARTED.
  EXPECT_EQ(GetSessionStateJson(), R"({"privacy_filter":true,"state":1})");

  // The scenario should also be "uploaded" (actually written to the output
  // file).
  TriggerScenarioAndWaitForOutput();
  EXPECT_TRUE(OutputFileExists());
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestFromCommandLine,
                       PRE_IgnoreThrottle) {
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  EXPECT_EQ(GetSessionStateJson(), R"({"privacy_filter":true,"state":1})");

  // This updates the upload time for the test scenario to the current time,
  // even though the output is actually written to a file.
  TriggerScenarioAndWaitForOutput();
  EXPECT_TRUE(OutputFileExists());

  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(state, R"({"privacy_filter":true,"state":3})"))
      << "Actual: " << state;
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestFromCommandLine,
                       IgnoreThrottle) {
  // The scenario from the command-line should be started even though not
  // enough time has elapsed since the last upload (set in the PRE_ above).
  ASSERT_FALSE(OutputFileExists());

  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 1 = STARTED.
  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(state, R"({"privacy_filter":true,"state":1})"))
      << "Actual: " << state;

  // The scenario should also be "uploaded" (actually written to the output
  // file).
  TriggerScenarioAndWaitForOutput();
  EXPECT_TRUE(OutputFileExists());
}

}  // namespace tracing
