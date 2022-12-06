// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_delegate.h"

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/background_tracing_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace android_webview {

bool IsBackgroundTracingCommandLine() {
  auto tracing_mode = tracing::GetBackgroundTracingSetupMode();
  if (tracing_mode == tracing::BackgroundTracingSetupMode::kFromConfigFile ||
      tracing_mode ==
          tracing::BackgroundTracingSetupMode::kFromFieldTrialLocalOutput) {
    return true;
  }
  return false;
}

AwTracingDelegate::AwTracingDelegate() {}
AwTracingDelegate::~AwTracingDelegate() {}

// static
void AwTracingDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(tracing::kBackgroundTracingSessionState);
}

bool AwTracingDelegate::IsAllowedToBeginBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced and uploaded.
  if (IsBackgroundTracingCommandLine())
    return true;

  // We call Initialize() only when a tracing scenario tries to start, and
  // unless this happens we never save state. In particular, if the background
  // tracing experiment is disabled, Initialize() will never be called, and we
  // will thus not save state. This means that when we save the background
  // tracing session state for one session, and then later read the state in a
  // future session, there might have been sessions between these two where
  // tracing was disabled. Therefore, the return value of
  // DidLastSessionEndUnexpectedly() might not be for the directly preceding
  // session, but instead it is the previous session where tracing was enabled.
  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();
  state.Initialize(AwBrowserProcess::GetInstance()->local_state());

  // Don't start a new trace if the previous trace did not end.
  if (state.DidLastSessionEndUnexpectedly()) {
    tracing::RecordDisallowedMetric(
        tracing::TracingFinalizationDisallowedReason::
            kLastTracingSessionDidNotEnd);
    return false;
  }

  // TODO(crbug.com/1290887): check the trace limit per week (to be implemented
  // later)

  state.NotifyTracingStarted();
  return true;
}

bool AwTracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data,
    bool is_crash_scenario) {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced and uploaded.
  if (IsBackgroundTracingCommandLine())
    return true;

  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();
  state.NotifyFinalizationStarted();

  // TODO(crbug.com/1290887): check the trace limit per week (to be implemented
  // later)

  state.OnScenarioUploaded(config.scenario_name());
  return true;
}

absl::optional<base::Value> AwTracingDelegate::GenerateMetadataDict() {
  base::Value::Dict metadata_dict;
  metadata_dict.Set("revision", version_info::GetLastChange());
  return base::Value(std::move(metadata_dict));
}

}  // namespace android_webview
