// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_handler.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/navigation_predictor/preloading_model_executor.h"

PreloadingModelHandler::PreloadingModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : ModelHandler<float, const std::vector<float>&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<PreloadingModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_PRELOADING_HEURISTICS,
          /*model_metadata=*/absl::nullopt) {
  SetShouldUnloadModelOnComplete(false);
}

PreloadingModelHandler::~PreloadingModelHandler() = default;
