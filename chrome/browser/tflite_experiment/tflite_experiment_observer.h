// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_OBSERVER_H_
#define CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace machine_learning {
class InProcessTFLitePredictor;
}  // namespace machine_learning

// Web content observer that runs a TFLite predictor
// at different stages of a navigation.
class TFLiteExperimentObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TFLiteExperimentObserver> {
 public:
  ~TFLiteExperimentObserver() override;

 private:
  friend class content::WebContentsUserData<TFLiteExperimentObserver>;
  explicit TFLiteExperimentObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Set input of the TFLite model for testing.
  void CreatePredictorInputForTesting();

  // Log data in TFLite experiment |log_path|.
  static void Log(base::Optional<std::string> log_path,
                  const std::string& data);

  // Writes header in TFLite experiment |log_path|.
  static void LogWriteHeader(base::Optional<std::string> log_path);

  // Writes TFLite experiment metrics in |log_path| when experiment is finished.
  static void LogDictionary(base::Optional<std::string> log_path,
                            const std::string&);

  // The predictor is capable of running a TFLite model.
  machine_learning::InProcessTFLitePredictor* tflite_predictor_ = nullptr;

  // True when |tflite_predictor_| ran model evaluation. It forces
  // the observer to run tflite prediction only once.
  bool is_tflite_evaluated_ = false;

  // Log dictionary that keeps recorded metrics.
  base::DictionaryValue log_dict_;

  // TFLite experiment log path.
  base::Optional<std::string> tflite_experiment_log_path_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_OBSERVER_H_
