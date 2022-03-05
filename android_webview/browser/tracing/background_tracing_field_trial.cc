// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/background_tracing_field_trial.h"

#include "base/callback_helpers.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace {

const char kBackgroundTracingFieldTrial[] = "BackgroundWebviewTracing";

void SetupBackgroundTracingFieldTrial(int allowed_modes) {
  auto* manager = content::BackgroundTracingManager::GetInstance();
  DCHECK(manager);
  std::unique_ptr<content::BackgroundTracingConfig> config =
      manager->GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);

  if (!config)
    return;

  if ((config->tracing_mode() & allowed_modes) == 0)
    return;

  // WebView-only tracing session has additional filtering of event names that
  // include package names as a privacy requirement (see
  // go/public-webview-trace-collection).
  config->SetPackageNameFilteringEnabled(
      config->tracing_mode() != content::BackgroundTracingConfig::SYSTEM);

  manager->SetActiveScenario(std::move(config),
                             content::BackgroundTracingManager::ANONYMIZE_DATA);
}

}  // namespace

namespace android_webview {

void MaybeSetupSystemTracing() {
  if (!tracing::ShouldSetupSystemTracing())
    return;

  SetupBackgroundTracingFieldTrial(content::BackgroundTracingConfig::SYSTEM);
}

void MaybeSetupWebViewOnlyTracing() {
  SetupBackgroundTracingFieldTrial(
      content::BackgroundTracingConfig::PREEMPTIVE |
      content::BackgroundTracingConfig::REACTIVE);
}

}  // namespace android_webview
