// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_

#include "chrome/browser/navigation_predictor/preloading_model_handler.h"
#include "components/keyed_service/core/keyed_service.h"

class OptimizationGuideKeyedService;

class PreloadingModelKeyedService : public KeyedService {
 public:
  PreloadingModelKeyedService(const PreloadingModelKeyedService&) = delete;
  explicit PreloadingModelKeyedService(
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  ~PreloadingModelKeyedService() override;

  PreloadingModelHandler* GetPreloadingModel() const {
    return preloading_model_handler_.get();
  }

 private:
  // preloading ML model
  std::unique_ptr<PreloadingModelHandler> preloading_model_handler_;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_
