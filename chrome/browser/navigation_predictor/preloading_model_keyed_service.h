// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

class OptimizationGuideKeyedService;
class PreloadingModelHandler;

class PreloadingModelKeyedService : public KeyedService {
 public:
  using Result = const std::optional<float>&;
  using ResultCallback = base::OnceCallback<void(Result)>;
  // These inputs are also recorded as part of
  // `NavigationPredictorModelTrainingData`. They are also similar to the fields
  // of `NavigationPredictorAnchorElementMetrics` and
  // `NavigationPredictorUserInteractions` UKM metrics. For more details please
  // check https://crsrc.org/c/tools/metrics/ukm/ukm.xml
  struct Inputs {
    Inputs();
    Inputs(const Inputs& other);
    Inputs& operator=(const Inputs& other);

    // Fields are arranged to minimize memory usage.
    bool contains_image : 1;
    bool has_text_sibling : 1;
    bool is_bold : 1;
    bool is_in_iframe : 1;
    bool is_url_incremented_by_one : 1;
    bool is_same_host : 1;
    bool is_in_viewport : 1 = true;
    bool is_pointer_hovering_over : 1 = true;
    uint8_t path_depth;
    uint8_t path_length;
    uint8_t font_size;
    uint8_t percent_clickable_area;
    int percent_vertical_distance;
    int pointer_hovering_over_count;
    base::TimeDelta navigation_start_to_link_logged;
    base::TimeDelta entered_viewport_to_left_viewport;
    base::TimeDelta hover_dwell_time;
  };

  PreloadingModelKeyedService(const PreloadingModelKeyedService&) = delete;
  explicit PreloadingModelKeyedService(
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  ~PreloadingModelKeyedService() override;

  void Score(base::CancelableTaskTracker* tracker,
             const Inputs& inputs,
             ResultCallback result_callback);

  // Runs |callback| now if |ModelAvailable()| or the next time |OnModelUpdated|
  // is called.
  void AddOnModelUpdatedCallbackForTesting(base::OnceClosure callback);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
 private:
  // preloading ML model
  std::unique_ptr<PreloadingModelHandler> preloading_model_handler_;
#endif
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_
