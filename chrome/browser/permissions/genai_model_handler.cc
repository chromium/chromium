// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/genai_model_handler.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"

namespace permissions {

GenAiModelHandler::GenAiModelHandler(
    OptimizationGuideKeyedService* optimization_guide)
    : optimization_guide_(optimization_guide) {}

GenAiModelHandler::~GenAiModelHandler() = default;

void GenAiModelHandler::OnDeviceModelAvailabilityChanged(
    optimization_guide::ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  // TODO(crbug.com/382447738) Implement this.
}

}  // namespace permissions
