// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_CHROME_RESPONSIVENESS_CALCULATOR_DELEGATE_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_CHROME_RESPONSIVENESS_CALCULATOR_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario.h"
#include "content/public/browser/responsiveness_calculator_delegate.h"

class UsageScenarioDataStore;
class UsageScenarioTracker;

// Emits different versions of the responsiveness metrics with an added suffix
// indicating the usage scenario that Chrome currently is in.
// See chrome/browser/metrics/usage_scenario/usage_scenario.h
// The embedder is responsible for creating instances of this class, but the
// ownership is passed to the //content layer.
class ChromeResponsivenessCalculatorDelegate
    : public content::ResponsivenessCalculatorDelegate {
 public:
  static std::unique_ptr<ChromeResponsivenessCalculatorDelegate> Create();

  static std::unique_ptr<ChromeResponsivenessCalculatorDelegate>
  CreateForTesting(UsageScenarioDataStore* usage_scenario_data_store);

  ~ChromeResponsivenessCalculatorDelegate() override;

  // content::ResponsivnessCalculatorDelegate:
  void OnMeasurementIntervalEnded() override;
  void OnResponsivenessEmitted(int num_congested_slices,
                               int min,
                               int exclusive_max,
                               size_t buckets) override;

 private:
  // Note: the parameter is exclusively for test usage.
  explicit ChromeResponsivenessCalculatorDelegate(
      UsageScenarioDataStore* usage_scenario_data_store);

  std::unique_ptr<UsageScenarioTracker> usage_scenario_tracker_;
  raw_ptr<UsageScenarioDataStore> usage_scenario_data_store_;
  std::optional<ScenarioParams> interval_scenario_params_;
  std::optional<size_t> extensions_with_content_scripts_in_interval_;
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_CHROME_RESPONSIVENESS_CALCULATOR_DELEGATE_H_
