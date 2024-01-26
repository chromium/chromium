// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_EXECUTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/base_model_executor.h"

// A model executor to run the history clusters module ranking model.
class PreloadingModelExecutor
    : public optimization_guide::BaseModelExecutor<float,
                                                   const std::vector<float>&> {
 public:
  using ModelInput = const std::vector<float>&;
  using ModelOutput = float;

  PreloadingModelExecutor();
  ~PreloadingModelExecutor() override;

  PreloadingModelExecutor(const PreloadingModelExecutor&) = delete;
  PreloadingModelExecutor& operator=(const PreloadingModelExecutor&) = delete;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  ModelInput input) override;
  std::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_EXECUTOR_H_
