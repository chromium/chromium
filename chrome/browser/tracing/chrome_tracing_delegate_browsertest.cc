// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpOnMainThread() override {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    content::TracingController::GetInstance();  // Create tracing agents.
  }
#endif

  bool StartPreemptiveScenario(
      content::BackgroundTracingManager::DataFiltering data_filtering,
      base::StringPiece scenario_name = "TestScenario") {
    base::DictionaryValue dict;

    dict.SetString("scenario_name", scenario_name);
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("custom_categories",
                   tracing::TraceStartupConfig::kDefaultStartupCategories);

    base::ListValue rules_list;
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test");
      rules_list.Append(std::move(rules_dict));
    }
    dict.SetKey("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(&dict));

    DCHECK(config);
    // Proto output is uploaded through
    // BackgroundTracingManager::SetTraceToUpload, with no ReceiveCallback.
    if (base::FeatureList::IsEnabled(features::kBackgroundTracingProtoOutput)) {
      return content::BackgroundTracingManager::GetInstance()
          ->SetActiveScenario(std::move(config), data_filtering);
    }

    // Legacy JSON output needs a receive callback.
    wait_for_upload_ = std::make_unique<base::RunLoop>();
    content::BackgroundTracingManager::ReceiveCallback receive_callback =
        base::BindRepeating(&ChromeTracingDelegateBrowserTest::OnUpload,
                            base::Unretained(this));

    return content::BackgroundTracingManager::GetInstance()
        ->SetActiveScenarioWithReceiveCallback(
            std::move(config), std::move(receive_callback), data_filtering);
  }

  void TriggerPreemptiveScenario(
      base::OnceClosure on_started_finalization_callback) {
    on_started_finalization_callback_ =
        std::move(on_started_finalization_callback);
    trigger_handle_ =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "test");

    content::BackgroundTracingManager::StartedFinalizingCallback
        started_finalizing_callback = base::BindOnce(
            &ChromeTracingDelegateBrowserTest::OnStartedFinalizing,
            base::Unretained(this));
    content::BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        trigger_handle_, std::move(started_finalizing_callback));
  }

  void WaitForUpload() {
    if (wait_for_upload_) {
      // Wait for the ReceiveCallback to quit this RunLoop.
      wait_for_upload_->Run();
      return;
    }

    // No ReceiveCallback set, so wait for SetTraceToUpload to be called.
    auto* manager = content::BackgroundTracingManager::GetInstance();
    while (!manager->HasTraceToUpload()) {
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_FALSE(manager->GetLatestTraceToUpload().empty());
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
  void OnUpload(std::unique_ptr<std::string> file_contents,
                content::BackgroundTracingManager::FinishedProcessingCallback
                    done_callback) {
    receive_count_ += 1;

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(done_callback), true));
    if (wait_for_upload_) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, wait_for_upload_->QuitClosure());
    }
  }

  void OnStartedFinalizing(bool success) {
    started_finalizations_count_++;
    last_on_started_finalizing_success_ = success;

    if (!on_started_finalization_callback_.is_null()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, std::move(on_started_finalization_callback_));
    }
  }

  std::unique_ptr<base::RunLoop> wait_for_upload_;
  base::OnceClosure on_started_finalization_callback_;
  int receive_count_;
  int started_finalizations_count_;
  content::BackgroundTracingManager::TriggerHandle trigger_handle_;
  bool last_on_started_finalizing_success_;
};

std::string GetSessionStateJson() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::DictionaryValue* state =
      local_state->GetDictionary(prefs::kBackgroundTracingSessionState);
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(*state, &json));
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

  content::BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  base::RunLoop wait_for_abort;
  content::BackgroundTracingManager::GetInstance()->WhenIdle(
      wait_for_abort.QuitClosure());
  wait_for_abort.Run();

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());

  // We should not be able to start a new reactive scenario immediately after
  // a previous one gets uploaded.
  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottledAfterPreviousDay) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  base::Time upload_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  upload_times["TestScenario"] = upload_time;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::NOT_ACTIVATED);

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

  base::Time upload_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  upload_times["TestScenario10"] = upload_time;
  upload_times["TestingScenario1"] = upload_time;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::NOT_ACTIVATED);

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

  base::Time upload_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  upload_times["TestScenario10"] = upload_time;
  upload_times["TestingScenario1"] = upload_time;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::NOT_ACTIVATED);

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

  base::Time upload_time = base::Time::Now() - base::TimeDelta::FromDays(8);
  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  upload_times["TestScenario"] = upload_time;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::NOT_ACTIVATED);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
  state = GetSessionStateJson();
  // Older entries are discarded.
  EXPECT_EQ(state, R"({"state":1,"upload_times":[]})");
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingUnexpectedSessionEnd) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times, ChromeTracingDelegate::BackgroundTracingState::STARTED);

  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingSessionRanLong) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::RAN_30_SECONDS);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingFinalizationStarted) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::FINALIZATION_STARTED);

  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingFinalizationBefore30Seconds) {
  std::string state = GetSessionStateJson();
  EXPECT_EQ(state, "{}");

  ChromeTracingDelegate::ScenarioUploadTimestampMap upload_times;
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::FINALIZATION_STARTED);

  // State does not update from finalization started to ran 30 seconds.
  ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
      upload_times,
      ChromeTracingDelegate::BackgroundTracingState::RAN_30_SECONDS);
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

class ChromeTracingDelegateBrowserTestOnStartup
    : public ChromeTracingDelegateBrowserTest {
 protected:
  ChromeTracingDelegateBrowserTestOnStartup() {}

  static std::string FieldTrialConfigTextFilter(
      const std::string& config_text) {
    // We need to replace the config JSON with the full one here, as we can't
    // pass JSON through the fieldtrial switch parsing.
    if (config_text == "default_config_for_testing") {
      return R"({
        "mode": "PREEMPTIVE_TRACING_MODE",
        "scenario_name": "TestScenario",
        "custom_categories": "base,toplevel",
        "configs": [{"rule": "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED",
                     "trigger_name": "test"}]
        })";
    }
    return config_text;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::VariationParamsManager::AppendVariationParams(
        "BackgroundTracing", "TestGroup",
        {{"config", "default_config_for_testing"}}, command_line);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    content::BackgroundTracingManager::GetInstance()
        ->SetConfigTextFilterForTesting(
            base::BindRepeating(&FieldTrialConfigTextFilter));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_ScenarioSetFromFieldtrial) {
  // This test would enable tracing and shutdown browser before 30 seconds
  // elapses. So, the profile would store incomplete state for next session.
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
  // State 1 = STARTED.
  EXPECT_EQ(GetSessionStateJson(), R"({"state":1,"upload_times":[]})");
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       ScenarioSetFromFieldtrial) {
  // Scenario should be inactive even though we have a config because last
  // session shut down unexpectedly.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
  // State 0 = NOT_ACTIVATED, current session is inactive.
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0,"upload_times":[]})");
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_StartupTracingThrottle) {
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
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
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
  std::string state = GetSessionStateJson();
  EXPECT_TRUE(base::MatchPattern(
      state,
      R"({"state":0,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << state;
}
