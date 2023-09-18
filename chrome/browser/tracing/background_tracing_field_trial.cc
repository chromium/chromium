// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include "components/tracing/common/background_tracing_utils.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"

namespace tracing {

namespace {

using content::BackgroundTracingConfig;
using content::BackgroundTracingManager;

const char kBackgroundTracingFieldTrial[] = "BackgroundTracing";

}  // namespace

bool SetupBackgroundTracingFieldTrial() {
  BackgroundTracingManager::DataFiltering data_filtering =
      BackgroundTracingManager::ANONYMIZE_DATA;
  if (tracing::HasBackgroundTracingOutputFile()) {
    data_filtering = BackgroundTracingManager::NO_DATA_FILTERING;
    if (!tracing::SetBackgroundTracingOutputFile()) {
      return false;
    }
  }
  auto tracing_mode = tracing::GetBackgroundTracingSetupMode();
  if (tracing_mode == BackgroundTracingSetupMode::kFromFieldTrial) {
    auto& manager = BackgroundTracingManager::GetInstance();
    return manager.SetActiveScenario(
        manager.GetBackgroundTracingConfig(kBackgroundTracingFieldTrial),
        data_filtering);
  } else if (tracing_mode !=
             BackgroundTracingSetupMode::kDisabledInvalidCommandLine) {
    return tracing::SetupBackgroundTracingFromCommandLine();
  }
  return false;
}

}  // namespace tracing
