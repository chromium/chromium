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
  if (tracing_mode ==
          tracing::BackgroundTracingSetupMode::kFromJsonConfigFile ||
      tracing_mode ==
          tracing::BackgroundTracingSetupMode::kFromProtoConfigFile) {
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

bool AwTracingDelegate::IsAllowedToStartScenario() const {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced and uploaded.
  if (IsBackgroundTracingCommandLine()) {
    return true;
  }

  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();

  // Don't start a new trace if the previous trace did not end.
  if (state.DidLastSessionEndUnexpectedly()) {
    tracing::RecordDisallowedMetric(
        tracing::TracingFinalizationDisallowedReason::
            kLastTracingSessionDidNotEnd);
    return false;
  }

  return true;
}

bool AwTracingDelegate::OnBackgroundTracingActive(
    bool requires_anonymized_data) {
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

  if (!IsAllowedToStartScenario()) {
    return false;
  }

  state.OnTracingStarted();
  return true;
}

bool AwTracingDelegate::OnBackgroundTracingIdle(bool requires_anonymized_data) {
  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();
  state.OnTracingStopped();
  return true;
}

std::optional<base::Value::Dict> AwTracingDelegate::GenerateMetadataDict() {
  base::Value::Dict metadata_dict;
  metadata_dict.Set("revision", version_info::GetLastChange());
  return metadata_dict;
}

}  // namespace android_webview
