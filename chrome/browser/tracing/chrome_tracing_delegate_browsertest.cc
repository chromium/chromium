// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace {

class ChromeTracingDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ChromeTracingDelegateBrowserTest()
      : receive_count_(0),
        started_finalizations_count_(0),
        last_on_started_finalizing_success_(false) {}

#if !defined(OS_CHROMEOS)
  void SetUpOnMainThread() override {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    content::TracingController::GetInstance();  // Create tracing agents.
  }
#endif

  bool StartPreemptiveScenario(
      content::BackgroundTracingManager::DataFiltering data_filtering) {
    base::DictionaryValue dict;

    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test");
      rules_list->Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(&dict));

    DCHECK(config);
    wait_for_upload_ = std::make_unique<base::RunLoop>();
    content::BackgroundTracingManager::ReceiveCallback receive_callback =
        base::BindRepeating(&ChromeTracingDelegateBrowserTest::OnUpload,
                            base::Unretained(this));

    return content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
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
    if (base::FeatureList::IsEnabled(features::kBackgroundTracingProtoOutput)) {
      while (!content::BackgroundTracingManager::GetInstance()
                  ->HasTraceToUpload()) {
        base::RunLoop().RunUntilIdle();
      }
      EXPECT_FALSE(content::BackgroundTracingManager::GetInstance()
                       ->GetLatestTraceToUpload()
                       .empty());
      receive_count_++;
    } else {
      wait_for_upload_->Run();
    }
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
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, wait_for_upload_->QuitClosure());
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

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottled) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerPreemptiveScenario(base::OnceClosure());

  WaitForUpload();

  EXPECT_TRUE(get_receive_count() == 1);

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Time last_upload_time = base::Time::FromInternalValue(
      local_state->GetInt64(prefs::kBackgroundTracingLastUpload));
  EXPECT_FALSE(last_upload_time.is_null());

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
                       BackgroundTracingThrottleTimeElapsed) {
  EXPECT_TRUE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerPreemptiveScenario(base::OnceClosure());

  WaitForUpload();

  EXPECT_TRUE(get_receive_count() == 1);

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Time last_upload_time = base::Time::FromInternalValue(
      local_state->GetInt64(prefs::kBackgroundTracingLastUpload));
  EXPECT_FALSE(last_upload_time.is_null());

  content::BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  base::RunLoop wait_for_abort;
  content::BackgroundTracingManager::GetInstance()->WhenIdle(
      wait_for_abort.QuitClosure());
  wait_for_abort.Run();
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
  EXPECT_FALSE(base::trace_event::TraceLog::GetInstance()->IsEnabled());

  EXPECT_FALSE(StartPreemptiveScenario(
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  // We move the last upload time to eight days in the past,
  // and at that point should be able to start a scenario again.
  base::Time new_upload_time = last_upload_time - base::TimeDelta::FromDays(8);
  local_state->SetInt64(prefs::kBackgroundTracingLastUpload,
                        new_upload_time.ToInternalValue());
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

  static void FieldTrialConfigTextFilter(std::string* config_text) {
    ASSERT_TRUE(config_text);
    // We need to replace the config JSON with the full one here, as we can't
    // pass JSON through the fieldtrial switch parsing.
    if (*config_text == "default_config_for_testing") {
      *config_text =
          "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
          "\"BENCHMARK\",\"configs\": [{\"rule\": "
          "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
          "\"test\"}]}";
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::VariationParamsManager::AppendVariationParams(
        "BackgroundTracing", "TestGroup",
        {{"config", "default_config_for_testing"}}, command_line);

    tracing::SetConfigTextFilterForTesting(&FieldTrialConfigTextFilter);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_ScenarioSetFromFieldtrial) {
  // This test exists just to make sure the browser is created at least once and
  // so a default profile is created. Then, the next time the browser is
  // created, kMetricsReportingEnabled is explicitly read from the profile and
  // the startup scenario can be activated.
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       ScenarioSetFromFieldtrial) {
  // We should reach this point without crashing.
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_PRE_StartupTracingThrottle) {
  // This test exists just to make sure the browser is created at least once and
  // so a default profile is created. Then, the next time the browser is
  // created, kMetricsReportingEnabled is explicitly read from the profile and
  // the startup scenario can be activated.
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_StartupTracingThrottle) {
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());

  // Simulate a trace upload.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetInt64(prefs::kBackgroundTracingLastUpload,
                        base::Time::Now().ToInternalValue());
}

// https://crbug.com/832981: The test is reenabled to check if flakiness still
// exists.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       StartupTracingThrottle) {
  // The startup scenario should *not* be started, since not enough
  // time has elapsed since the last upload (set in the PRE_ above).
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance()->HasActiveScenario());
}

}  // namespace
