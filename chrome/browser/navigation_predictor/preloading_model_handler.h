// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_HANDLER_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_HANDLER_H_

#include "components/optimization_guide/core/model_handler.h"

// Model handler used to retrieve and eventually execute the model.
class PreloadingModelHandler
    : public optimization_guide::ModelHandler<float,
                                              const std::vector<float>&> {
 public:
  explicit PreloadingModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~PreloadingModelHandler() override;
  PreloadingModelHandler(const PreloadingModelHandler&) = delete;
  PreloadingModelHandler& operator=(const PreloadingModelHandler&) = delete;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_HANDLER_H_
