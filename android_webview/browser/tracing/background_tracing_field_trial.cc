// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/background_tracing_field_trial.h"

#include "base/callback_helpers.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace android_webview {

const char kBackgroundTracingFieldTrial[] = "BackgroundWebviewTracing";

void SetupBackgroundTracingFieldTrial() {
  std::unique_ptr<content::BackgroundTracingConfig> config =
      content::BackgroundTracingManager::GetInstance()
          ->GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);

  if (config &&
      config->tracing_mode() == content::BackgroundTracingConfig::SYSTEM &&
      tracing::ShouldSetupSystemTracing()) {
    // Only enable background tracing for system tracing if the system producer
    // is enabled.
    content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config),
        base::BindRepeating(
            base::DoNothing::Repeatedly<std::unique_ptr<std::string>,
                                        content::BackgroundTracingManager::
                                            FinishedProcessingCallback>()),
        content::BackgroundTracingManager::ANONYMIZE_DATA);
  }
}

}  // namespace android_webview