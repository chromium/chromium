// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
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
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/pref_names.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/tracing/public/cpp/tracing_features.h"

class ChromeTracingDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ChromeTracingDelegateBrowserTest()
      : receive_count_(0),
        started_finalizations_count_(0),
        last_on_started_finalizing_success_(false) {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    tracing::BackgroundTracingStateManager::GetInstance()
        .SetPrefServiceForTesting(local_state);
    content::TracingController::GetInstance();  // Create tracing agents.
  }

  bool StartPreemptiveScenario(
      content::BackgroundTracingManager::DataFiltering data_filtering,
      base::StringPiece scenario_name = "TestScenario",
      bool with_crash_scenario = false) {
    base::Value::Dict dict;

    dict.Set("scenario_name", scenario_name);
    dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
    dict.Set("custom_categories",
             tracing::TraceStartupConfig::kDefaultStartupCategories);

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
      base::StringPiece scenario_name = "TestScenario") {
    return StartPreemptiveScenario(data_filtering, scenario_name,
                                   /*with_crash_scenario=*/true);
  }

  void TriggerPreemptiveScenario(
      base::OnceClosure on_started_finalization_callback,
      base::StringPiece trigger_name = "test") {
    on_started_finalization_callback_ =
        std::move(on_started_finalization_callback);
    trigger_handle_ =
        content::BackgroundTracingManager::GetInstance().RegisterTriggerType(
            trigger_name);

    content::BackgroundTracingManager::StartedFinalizingCallback
        started_finalizing_callback = base::BindOnce(
            &ChromeTracingDelegateBrowserTest::OnStartedFinalizing,
            base::Unretained(this));
    content::BackgroundTracingManager::GetInstance().TriggerNamedEvent(
        trigger_handle_, std::move(started_finalizing_callback));
  }

  void TriggerPreemptiveScenarioWithCrash(
      base::OnceClosure on_started_finalization_callback) {
    TriggerPreemptiveScenario(std::move(on_started_finalization_callback),
                              "test_crash");
  }

  void WaitForUpload() {
    // No ReceiveCallback set, so wait for SetTraceToUpload to be called.
    auto& manager = content::BackgroundTracingManager::GetInstance();
    while (!manager.HasTraceToUpload()) {
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_FALSE(manager.GetLatestTraceToUpload().empty());
    receive_count_++;
  }

  int get_receive_count() const { return receive_count_; }
  bool get_started_finalizations() const {
    return started_finalizations_count_;
  }
  bool get_last_started_finalization_success() const {
    return last_on_started_finalizing_success_;
  }

 private:
  void OnStartedFinalizing(bool success) {
    started_finalizations_count_++;
    last_on_started_finalizing_success_ = success;

    if (!on_started_finalization_callback_.is_null()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, std::move(on_started_finalization_callback_));
    }
  }

  base::OnceClosure on_started_finalization_callback_;
  int receive_count_;
  int started_finalizations_count_;
  content::BackgroundTracingManager::TriggerHandle trigger_handle_;
  bool last_on_started_finalizing_success_;
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

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottled) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerPreemptiveScenario(base::OnceClosure());

  WaitForUpload();
  EXPECT_TRUE(get_receive_count() == 1);

  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":3,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;

  content::BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  base::RunLoop wait_for_abort;
  content::BackgroundTracingManager::GetInstance().WhenIdle(
      wait_for_abort.QuitClosure());
  wait_for_abort.Run();

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
#endif

  // We should not be able to start a new reactive scenario immediately after
  // a previous one gets uploaded.
  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottledAfterPreviousDay) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  base::Time upload_time = base::Time::Now() - base::Days(1);
  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  upload_times["TestScenario"] = upload_time;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::NOT_ACTIVATED);

  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":0,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottledUpdatedScenario) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  base::Time upload_time = base::Time::Now() - base::Days(1);
  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  upload_times["TestScenario10"] = upload_time;
  upload_times["TestingScenario1"] = upload_time;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::NOT_ACTIVATED);

  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING, "TestScenario12"));

  state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":0,"upload_times":[{"scenario":"TestScenario","time":"*"},)"
      R"({"scenario":"TestingScenario","time":"*"}]})"))
      << "Actual: " << state;
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottledDifferentScenario) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  base::Time upload_time = base::Time::Now() - base::Days(1);
  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  upload_times["TestScenario10"] = upload_time;
  upload_times["TestingScenario1"] = upload_time;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::NOT_ACTIVATED);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING, "OtherScenario"));

  state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":1,"upload_times":[{"scenario":"TestScenario","time":"*"},)"
      R"({"scenario":"TestingScenario","time":"*"}]})"))
      << "Actual: " << state;

  TriggerPreemptiveScenario(base::OnceClosure());

  WaitForUpload();
  EXPECT_TRUE(get_receive_count() == 1);

  state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":3,"upload_times":[{"scenario":"OtherScenario","time":"*"},)"
      R"({"scenario":"TestScenario","time":"*"},)"
      R"({"scenario":"TestingScenario","time":"*"}]})"))
      << "Actual: " << state;
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingThrottleTimeElapsed) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  base::Time upload_time = base::Time::Now() - base::Days(8);
  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  upload_times["TestScenario"] = upload_time;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::NOT_ACTIVATED);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
  state = GetSessionStateJson();
  // Older entries are discarded.
  EXPECT_EQ(state, R"({"state":1,"upload_times":[]})");
}

// Test how crash scenarios behave when uploads are throttled: tracing starts if
// a crash scenario exists, and the trace is uploaded if the crash scenario is
// triggered.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingCrashScenarioNotThrottled) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerPreemptiveScenario(base::OnceClosure());

  WaitForUpload();
  EXPECT_EQ(get_receive_count(), 1);

  content::BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  base::RunLoop wait_for_abort;
  content::BackgroundTracingManager::GetInstance().WhenIdle(
      wait_for_abort.QuitClosure());
  wait_for_abort.Run();

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  // We should immediately be able to start a new scenario that includes a
  // crash scenario.
  EXPECT_TRUE(StartPreemptiveScenarioWithCrash(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
  TriggerPreemptiveScenarioWithCrash(base::OnceClosure());

  WaitForUpload();
  EXPECT_EQ(get_receive_count(), 2);
}

// Test how crash scenarios behave when uploads are throttled: tracing starts if
// a crash scenario exists, but if a different scenario is triggered the upload
// should still be throttled.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingCrashScenarioUploadThrottled) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerPreemptiveScenario(base::OnceClosure());

  WaitForUpload();
  EXPECT_EQ(get_receive_count(), 1);

  content::BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  base::RunLoop wait_for_abort;
  content::BackgroundTracingManager::GetInstance().WhenIdle(
      wait_for_abort.QuitClosure());
  wait_for_abort.Run();

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  // We should immediately be able to start a new scenario that includes a
  // crash scenario.
  EXPECT_TRUE(StartPreemptiveScenarioWithCrash(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  base::RunLoop wait_for_finalization_start;
  TriggerPreemptiveScenario(wait_for_finalization_start.QuitClosure());
  wait_for_finalization_start.Run();

  EXPECT_EQ(get_started_finalizations(), 1);
  EXPECT_FALSE(get_last_started_finalization_success());
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingUnexpectedSessionEnd) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::STARTED);

  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingSessionRanLong) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::RAN_30_SECONDS);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingFinalizationStarted) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::FINALIZATION_STARTED);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingFinalizationBefore30Seconds) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  tracing::BackgroundTracingStateManager::ScenarioUploadTimestampMap
      upload_times;
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::FINALIZATION_STARTED);

  // State does not update from finalization started to ran 30 seconds.
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      upload_times, tracing::BackgroundTracingState::RAN_30_SECONDS);
  state = GetSessionStateJson();
  EXPECT_EQ(state, R"({"state":2,"upload_times":[]})");
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

  base::RunLoop wait_for_finalization_start;
  TriggerPreemptiveScenario(wait_for_finalization_start.QuitClosure());
  wait_for_finalization_start.Run();

  EXPECT_TRUE(get_started_finalizations() == 1);
  EXPECT_FALSE(get_last_started_finalization_success());
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

  base::RunLoop wait_for_finalization_start;
  TriggerPreemptiveScenario(wait_for_finalization_start.QuitClosure());
  wait_for_finalization_start.Run();

  EXPECT_TRUE(get_started_finalizations() == 1);
  EXPECT_FALSE(get_last_started_finalization_success());
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
        "BackgroundTracing", "TestGroup",
        {{"config", "default_config_for_testing"}});
  }

  static std::string FieldTrialConfigTextFilter(
      const std::string& config_text) {
    // We need to replace the config JSON with the full one here, as we can't
    // pass JSON through the fieldtrial switch parsing.
    if (config_text == "default_config_for_testing") {
      return kDefaultConfigText;
    }
    return config_text;
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    content::BackgroundTracingManager::GetInstance()
        .SetConfigTextFilterForTesting(
            base::BindRepeating(&FieldTrialConfigTextFilter));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_ScenarioSetFromFieldtrial) {
  // This test would enable tracing and shutdown browser before 30 seconds
  // elapses. So, the profile would store incomplete state for next session.
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 1 = STARTED.
  EXPECT_EQ(GetSessionStateJson(), R"({"state":1,"upload_times":[]})");
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       ScenarioSetFromFieldtrial) {
  // Scenario should be inactive even though we have a config because last
  // session shut down unexpectedly.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 0 = NOT_ACTIVATED, current session is inactive.
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0,"upload_times":[]})");
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_StartupTracingThrottle) {
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  EXPECT_EQ(GetSessionStateJson(), R"({"state":1,"upload_times":[]})");

  TriggerPreemptiveScenario(base::OnceClosure());

  // This updates the upload time for the test scenario to current time.
  WaitForUpload();
  EXPECT_TRUE(get_receive_count() == 1);

  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":3,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       StartupTracingThrottle) {
  // The startup scenario should *not* be started, since not enough
  // time has elapsed since the last upload (set in the PRE_ above).
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":0,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;
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
    command_line->AppendSwitchPath("enable-background-tracing", config_path);

    output_path_ = base::FilePath(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("output.perfetto.gz")));
    command_line->AppendSwitchPath("background-tracing-output-file",
                                   output_path_);
  }

  bool OutputPathExists() const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(output_path_);
  }

  void TriggerScenarioAndWaitForOutput() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Wait for the output file to appear instead of for the trigger callback
    // (which just means the data is ready to write).
    base::FilePathWatcher output_watcher;
    base::RunLoop run_loop;
    output_watcher.Watch(
        output_path_, base::FilePathWatcher::Type::kNonRecursive,
        base::BindLambdaForTesting(
            [&run_loop](const base::FilePath&, bool) { run_loop.Quit(); }));
    TriggerPreemptiveScenario(base::OnceClosure());
    run_loop.Run();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath output_path_;
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestFromCommandLine,
                       ScenarioFromCommandLine) {
  ASSERT_FALSE(OutputPathExists());

  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 1 = STARTED.
  EXPECT_EQ(GetSessionStateJson(), R"({"state":1,"upload_times":[]})");

  // The scenario should also be "uploaded" (actually written to the output
  // file).
  TriggerScenarioAndWaitForOutput();
  EXPECT_TRUE(OutputPathExists());
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestFromCommandLine,
                       PRE_IgnoreThrottle) {
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  EXPECT_EQ(GetSessionStateJson(), R"({"state":1,"upload_times":[]})");

  // This updates the upload time for the test scenario to the current time,
  // even though the output is actually written to a file.
  TriggerScenarioAndWaitForOutput();
  EXPECT_TRUE(OutputPathExists());

  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":3,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestFromCommandLine,
                       IgnoreThrottle) {
  // The scenario from the command-line should be started even though not
  // enough time has elapsed since the last upload (set in the PRE_ above).
  ASSERT_FALSE(OutputPathExists());

  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  // State 1 = STARTED.
  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":1,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;

  // The scenario should also be "uploaded" (actually written to the output
  // file).
  TriggerScenarioAndWaitForOutput();
  EXPECT_TRUE(OutputPathExists());
}
