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

void SetupBackgroundTracingFieldTrial() {
  if (tracing::GetBackgroundTracingSetupMode() ==
      BackgroundTracingSetupMode::kDisabledInvalidCommandLine)
    return;

  if (tracing::SetupBackgroundTracingFromCommandLine(
          kBackgroundTracingFieldTrial))
    return;

  auto& manager = BackgroundTracingManager::GetInstance();
  manager.SetActiveScenario(
      manager.GetBackgroundTracingConfig(kBackgroundTracingFieldTrial),
      BackgroundTracingManager::ANONYMIZE_DATA);
}

}  // namespace tracing
