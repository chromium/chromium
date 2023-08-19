// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

PreloadingModelKeyedService::PreloadingModelKeyedService(
    OptimizationGuideKeyedService* optimization_guide_keyed_service) {
  auto* model_provider =
      static_cast<optimization_guide::OptimizationGuideModelProvider*>(
          optimization_guide_keyed_service);

  if (model_provider) {
    preloading_model_handler_ =
        std::make_unique<PreloadingModelHandler>(model_provider);
  }
}
PreloadingModelKeyedService::~PreloadingModelKeyedService() = default;
