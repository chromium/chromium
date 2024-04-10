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

AwTracingDelegate::AwTracingDelegate()
    : state_manager_(tracing::BackgroundTracingStateManager::CreateInstance(
          AwBrowserProcess::GetInstance()->local_state())) {}
AwTracingDelegate::AwTracingDelegate(
    std::unique_ptr<tracing::BackgroundTracingStateManager> state_manager)
    : state_manager_(std::move(state_manager)) {}
AwTracingDelegate::~AwTracingDelegate() = default;

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
  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();

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

}  // namespace android_webview
