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
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace android_webview {

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

bool AwTracingDelegate::OnBackgroundTracingActive(
    bool requires_anonymized_data) {
  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();

  state.OnTracingStarted();
  return true;
}

void AwTracingDelegate::OnBackgroundTracingIdle() {
  tracing::BackgroundTracingStateManager& state =
      tracing::BackgroundTracingStateManager::GetInstance();
  state.OnTracingStopped();
}

}  // namespace android_webview
