// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

OptimizationGuideGlobalStateHolderKeyedService::
    OptimizationGuideGlobalStateHolderKeyedService() = default;

OptimizationGuideGlobalStateHolderKeyedService::
    ~OptimizationGuideGlobalStateHolderKeyedService() = default;

optimization_guide::OptimizationGuideGlobalState&
OptimizationGuideGlobalStateHolderKeyedService::GetGlobalState() {
  if (!optimization_guide_global_state_) {
    optimization_guide_global_state_ =
        optimization_guide::OptimizationGuideGlobalState::CreateOrGet();
  }
  return *optimization_guide_global_state_;
}
