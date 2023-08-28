// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

PreloadingModelKeyedService::Inputs::Inputs() = default;

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

void PreloadingModelKeyedService::AddOnModelUpdatedCallback(
    base::OnceClosure callback) {
  if (!preloading_model_handler_) {
    return;
  }
  preloading_model_handler_->AddOnModelUpdatedCallback(std::move(callback));
}

void PreloadingModelKeyedService::Score(base::CancelableTaskTracker* tracker,
                                        const Inputs& inputs,
                                        ResultCallback result_callback) {
  if (!preloading_model_handler_ ||
      !preloading_model_handler_->ModelAvailable()) {
    std::move(result_callback).Run(absl::nullopt);
    return;
  }

  std::vector<float> model_input{
      /* input 0 */ inputs.contains_image ? 1.0f : 0.0f,
      /* input 1 */ static_cast<float>(inputs.font_size),
      /* input 2 */ inputs.has_text_sibling ? 1.0f : 0.0f,
      /* input 3 */ inputs.is_bold ? 1.0f : 0.0f,
      /* input 4 */ inputs.is_in_iframe ? 1.0f : 0.0f,
      /* input 5 */ inputs.is_url_incremented_by_one ? 1.0f : 0.0f,
      /* input 6 */
      static_cast<float>(
          inputs.navigation_start_to_link_logged.InMillisecondsF()),
      /* input 7 */ static_cast<float>(inputs.path_depth),
      /* input 8 */ static_cast<float>(inputs.path_length),
      /* input 9 */ static_cast<float>(inputs.percent_clickable_area),
      /* input 10*/ static_cast<float>(inputs.percent_vertical_distance),
      /* input 11*/ inputs.is_same_origin ? 1.0f : 0.0f,
      /* input 12*/ inputs.is_in_viewport ? 1.0f : 0.0f,
      /* input 13*/ inputs.is_pointer_hovering_over ? 1.0f : 0.0f,
      /* input 14*/
      static_cast<float>(
          inputs.entered_viewport_to_left_viewport.InMillisecondsF()),
      /* input 15*/
      static_cast<float>(inputs.hover_dwell_time.InMillisecondsF()),
      /* input 16*/ static_cast<float>(inputs.pointer_hovering_over_count)};

  preloading_model_handler_->ExecuteModelWithInput(
      tracker, std::move(result_callback), model_input);
}
