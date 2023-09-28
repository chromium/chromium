// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/background_tracing_field_trial.h"

#include <utility>

#include "components/tracing/common/background_tracing_utils.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace android_webview {

namespace {

using content::BackgroundTracingConfig;
using content::BackgroundTracingManager;

const char kBackgroundTracingFieldTrial[] = "BackgroundWebviewTracing";

}  // namespace

bool MaybeSetupSystemTracingFromFieldTrial() {
  if (tracing::GetBackgroundTracingSetupMode() !=
      tracing::BackgroundTracingSetupMode::kFromFieldTrial) {
    return false;
  }

  auto& manager = BackgroundTracingManager::GetInstance();
  std::unique_ptr<BackgroundTracingConfig> config =
      manager.GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);
  if (!config || config->tracing_mode() != BackgroundTracingConfig::SYSTEM) {
    return false;
  }

  BackgroundTracingManager::DataFiltering data_filtering =
      BackgroundTracingManager::ANONYMIZE_DATA;
  if (tracing::HasBackgroundTracingOutputFile()) {
    data_filtering = BackgroundTracingManager::NO_DATA_FILTERING;
    if (!tracing::SetBackgroundTracingOutputFile()) {
      return false;
    }
  }

  return manager.SetActiveScenario(std::move(config), data_filtering);
}

bool MaybeSetupWebViewOnlyTracingFromFieldTrial() {
  if (tracing::GetBackgroundTracingSetupMode() !=
      tracing::BackgroundTracingSetupMode::kFromFieldTrial) {
    return false;
  }

  // WebView-only tracing session has additional filtering of event names that
  // include package names as a privacy requirement (see
  // go/public-webview-trace-collection).
  BackgroundTracingManager::DataFiltering data_filtering =
      BackgroundTracingManager::ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME;
  if (tracing::HasBackgroundTracingOutputFile()) {
    data_filtering = BackgroundTracingManager::NO_DATA_FILTERING;
    if (!tracing::SetBackgroundTracingOutputFile()) {
      return false;
    }
  }

  auto& manager = BackgroundTracingManager::GetInstance();
  auto field_tracing_config = tracing::GetFieldTracingConfig();
  if (field_tracing_config) {
    return manager.InitializeScenarios(std::move(*field_tracing_config),
                                       data_filtering);
  }
  std::unique_ptr<BackgroundTracingConfig> config =
      manager.GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);
  if (config && config->tracing_mode() == BackgroundTracingConfig::SYSTEM) {
    return false;
  }
  return manager.SetActiveScenario(std::move(config), data_filtering);
}

}  // namespace android_webview
