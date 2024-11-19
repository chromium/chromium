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
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/test/bind.h"
#include "base/test/test_proto_loader.h"
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

perfetto::protos::gen::ChromeFieldTracingConfig ParseFieldTracingConfigFromText(
    const std::string& proto_text) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.ChromeFieldTracingConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  perfetto::protos::gen::ChromeFieldTracingConfig destination;
  destination.ParseFromString(serialized_message);
  return destination;
}

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

  bool StartScenario(
      content::BackgroundTracingManager::DataFiltering data_filtering) {
    constexpr const char kScenarioConfig[] = R"pb(
      scenarios: {
        scenario_name: "test_scenario"
        start_rules: { manual_trigger_name: "start_trigger" }
        upload_rules: { manual_trigger_name: "upload_trigger" }
        trace_config: {
          data_sources: {
            config: {
              name: "track_event"
              track_event_config: {
                disabled_categories: [ "*" ],
                enabled_categories: [ "toplevel" ]
              }
            }
          }
          data_sources: { config: { name: "org.chromium.trace_metadata" } }
        }
      }
    )pb";

    EXPECT_TRUE(content::BackgroundTracingManager::GetInstance()
                    .InitializeFieldScenarios(
                        ParseFieldTracingConfigFromText(kScenarioConfig),
                        data_filtering, false, 0));

    return base::trace_event::EmitNamedTrigger("start_trigger");
  }

  void TriggerPreemptiveScenario(
      const std::string& trigger_name = "upload_trigger") {
    base::trace_event::EmitNamedTrigger(trigger_name);
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
                       BackgroundTracingSessionRanLong) {
  base::Value::Dict dict;
  dict.Set("state",
           static_cast<int>(tracing::BackgroundTracingState::RAN_30_SECONDS));
  SetSessionState(std::move(dict));
  tracing::BackgroundTracingStateManager::GetInstance().ResetForTesting();

  EXPECT_TRUE(
      StartScenario(content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingFinalizationStarted) {
  base::Value::Dict dict;
  dict.Set("state", static_cast<int>(
                        tracing::BackgroundTracingState::FINALIZATION_STARTED));
  SetSessionState(std::move(dict));
  tracing::BackgroundTracingStateManager::GetInstance().ResetForTesting();

  EXPECT_TRUE(
      StartScenario(content::BackgroundTracingManager::NO_DATA_FILTERING));
}

// If we need a PII-stripped trace, any existing OTR session should block the
// trace.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       ExistingIncognitoSessionBlockingTraceStart) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());
  EXPECT_FALSE(
      StartScenario(content::BackgroundTracingManager::ANONYMIZE_DATA));
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
  EXPECT_TRUE(StartScenario(content::BackgroundTracingManager::ANONYMIZE_DATA));

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
  EXPECT_TRUE(StartScenario(content::BackgroundTracingManager::ANONYMIZE_DATA));

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());
  CloseBrowserSynchronously(incognito_browser);
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserActive());

  TestBackgroundTracingHelper background_tracing_helper;
  TriggerPreemptiveScenario();
  background_tracing_helper.WaitForScenarioIdle();
}

}  // namespace tracing
