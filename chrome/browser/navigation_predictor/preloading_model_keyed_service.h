// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_

#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class OptimizationGuideKeyedService;
class PreloadingModelHandler;

class PreloadingModelKeyedService : public KeyedService {
 public:
  using Result = const absl::optional<float>&;
  using ResultCallback = base::OnceCallback<void(Result)>;
  struct Inputs {
    Inputs();
    // These inputs are similar to the fields of
    // `NavigationPredictorAnchorElementMetrics` UKM metrics. For more details
    // please check https://crsrc.org/c/tools/metrics/ukm/ukm.xml
    bool contains_image;
    int font_size;
    bool has_text_sibling;
    bool is_bold;
    bool is_in_iframe;
    bool is_url_incremented_by_one;
    base::TimeDelta navigation_start_to_link_logged;
    int path_depth;
    int path_length;
    double percent_clickable_area;
    double percent_vertical_distance;
    bool is_same_origin;
    // These inputs are similar to the fields of
    // `NavigationPredictorUserInteractions` UKM metrics. For more details
    // please check https://crsrc.org/c/tools/metrics/ukm/ukm.xml
    bool is_in_viewport = true;
    bool is_pointer_hovering_over = true;
    base::TimeDelta entered_viewport_to_left_viewport;
    base::TimeDelta hover_dwell_time;
    int pointer_hovering_over_count;
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
  void AddOnModelUpdatedCallback(base::OnceClosure callback);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
 private:
  // preloading ML model
  std::unique_ptr<PreloadingModelHandler> preloading_model_handler_;
#endif
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_H_
