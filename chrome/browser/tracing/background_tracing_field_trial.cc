// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "services/tracing/public/cpp/trace_startup_config.h"

namespace tracing {

namespace {

using content::BackgroundTracingConfig;
using content::BackgroundTracingManager;

const char kBackgroundTracingFieldTrial[] = "BackgroundTracing";

}  // namespace

bool MaybeSetupSystemTracingFromFieldTrial() {
  if (tracing::GetBackgroundTracingSetupMode() !=
      BackgroundTracingSetupMode::kFromFieldTrial) {
    return false;
  }

  auto& manager = BackgroundTracingManager::GetInstance();
  auto trigger_config = tracing::GetTracingTriggerRulesConfig();
  if (trigger_config) {
    return manager.InitializePerfettoTriggerRules(std::move(*trigger_config));
  }
  if (tracing::IsFieldTracingEnabled()) {
    return false;
  }
  std::unique_ptr<BackgroundTracingConfig> config =
      manager.GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);
  if (!config || config->tracing_mode() != BackgroundTracingConfig::SYSTEM) {
    return false;
  }

  BackgroundTracingManager::DataFiltering data_filtering =
      BackgroundTracingManager::ANONYMIZE_DATA;
  if (tracing::HasBackgroundTracingOutputPath()) {
    data_filtering = BackgroundTracingManager::NO_DATA_FILTERING;
    if (!tracing::SetBackgroundTracingOutputPath()) {
      return false;
    }
  }

  return manager.SetActiveScenario(std::move(config), data_filtering);
}

bool MaybeSetupBackgroundTracingFromFieldTrial() {
  if (tracing::GetBackgroundTracingSetupMode() !=
      BackgroundTracingSetupMode::kFromFieldTrial) {
    return false;
  }

  BackgroundTracingManager::DataFiltering data_filtering =
      BackgroundTracingManager::ANONYMIZE_DATA;
  if (tracing::HasBackgroundTracingOutputPath()) {
    data_filtering = BackgroundTracingManager::NO_DATA_FILTERING;
    if (!tracing::SetBackgroundTracingOutputPath()) {
      return false;
    }
  } else if (!tracing::ShouldAnonymizeFieldTracing()) {
    data_filtering = BackgroundTracingManager::NO_DATA_FILTERING;
  }

  tracing::TraceStartupConfig::GetInstance().SetBackgroundStartupTracingEnabled(
      tracing::ShouldTraceStartup());

  auto& manager = BackgroundTracingManager::GetInstance();
  auto field_tracing_config = tracing::GetFieldTracingConfig();
  if (field_tracing_config) {
    return manager.InitializeFieldScenarios(std::move(*field_tracing_config),
                                            data_filtering);
  }

  std::unique_ptr<BackgroundTracingConfig> config =
      manager.GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);
  if (config && config->tracing_mode() == BackgroundTracingConfig::SYSTEM) {
    return false;
  }
  return manager.SetActiveScenario(std::move(config), data_filtering);
}

}  // namespace tracing
