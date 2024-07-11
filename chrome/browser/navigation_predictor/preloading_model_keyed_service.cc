// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/navigation_predictor/preloading_model_handler.h"
#endif

namespace {

// The model takes all of its inputs as floats, so this is a convenience
// function for turning various types into floats.
template <typename T>
constexpr float ToInput(T val) {
  return static_cast<float>(val);
}

template <>
constexpr float ToInput(base::TimeDelta val) {
  return static_cast<float>(val.InMillisecondsF());
}

static_assert(1.0f == ToInput(true));

}  // namespace

PreloadingModelKeyedService::Inputs::Inputs() = default;
PreloadingModelKeyedService::Inputs::Inputs(const Inputs& other) = default;
PreloadingModelKeyedService::Inputs&
PreloadingModelKeyedService::Inputs::operator=(const Inputs& other) = default;

PreloadingModelKeyedService::PreloadingModelKeyedService(
    OptimizationGuideKeyedService* optimization_guide_keyed_service) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  auto* model_provider =
      static_cast<optimization_guide::OptimizationGuideModelProvider*>(
          optimization_guide_keyed_service);

  if (model_provider) {
    preloading_model_handler_ =
        std::make_unique<PreloadingModelHandler>(model_provider);
  }
#endif
}

PreloadingModelKeyedService::~PreloadingModelKeyedService() = default;

void PreloadingModelKeyedService::AddOnModelUpdatedCallbackForTesting(
    base::OnceClosure callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  CHECK(preloading_model_handler_);
  preloading_model_handler_->AddOnModelUpdatedCallback(std::move(callback));
#endif
}

void PreloadingModelKeyedService::Score(base::CancelableTaskTracker* tracker,
                                        const Inputs& inputs,
                                        ResultCallback result_callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!preloading_model_handler_ ||
      !preloading_model_handler_->ModelAvailable()) {
    std::move(result_callback).Run(std::nullopt);
    return;
  }

  std::vector<float> model_input{
      /* input 0 */ ToInput(inputs.contains_image),
      /* input 1 */ ToInput(inputs.font_size),
      /* input 2 */ ToInput(inputs.has_text_sibling),
      /* input 3 */ ToInput(inputs.is_bold),
      /* input 4 */ ToInput(inputs.is_in_iframe),
      /* input 5 */ ToInput(inputs.is_url_incremented_by_one),
      /* input 6 */
      ToInput(inputs.navigation_start_to_link_logged),
      /* input 7 */ ToInput(inputs.path_depth),
      /* input 8 */ ToInput(inputs.path_length),
      /* input 9 */ ToInput(inputs.percent_clickable_area),
      /* input 10*/ ToInput(inputs.percent_vertical_distance),
      /* input 11*/ ToInput(inputs.is_same_host),
      /* input 12*/ ToInput(inputs.is_in_viewport),
      /* input 13*/ ToInput(inputs.is_pointer_hovering_over),
      /* input 14*/
      ToInput(inputs.entered_viewport_to_left_viewport),
      /* input 15*/
      ToInput(inputs.hover_dwell_time),
      /* input 16*/ ToInput(inputs.pointer_hovering_over_count)};

  preloading_model_handler_->ExecuteModelWithInput(
      tracker, std::move(result_callback), model_input);
#else
  std::move(result_callback).Run(std::nullopt);
#endif
}
