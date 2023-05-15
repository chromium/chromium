// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_responsiveness_calculator_delegate.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"

namespace {

bool IsChromeUsedInScenario(Scenario scenario) {
  // Exclude all scenarios in which Chrome is not being used. Defined as either
  // visible to the user, playing audio, or capturing video.
  switch (scenario) {
    case Scenario::kAllTabsHiddenNoVideoCaptureOrAudio:
    case Scenario::kZeroWindow:
      return false;

    case Scenario::kAllTabsHiddenAudio:
    case Scenario::kAllTabsHiddenVideoCapture:
    case Scenario::kAudio:
    case Scenario::kEmbeddedVideoNoNavigation:
    case Scenario::kEmbeddedVideoWithNavigation:
    case Scenario::kFullscreenVideo:
    case Scenario::kInteraction:
    case Scenario::kNavigation:
    case Scenario::kPassive:
    case Scenario::kVideoCapture:
      return true;

    case Scenario::kAllTabsHiddenNoVideoCaptureOrAudioRecent:
    case Scenario::kZeroWindowRecent:
      // Short scenario only.
      NOTREACHED_NORETURN();
  }
}

}  // namespace

// static
std::unique_ptr<ChromeResponsivenessCalculatorDelegate>
ChromeResponsivenessCalculatorDelegate::Create() {
  // The instance will create its own data store if one is not provided.
  return base::WrapUnique(new ChromeResponsivenessCalculatorDelegate(
      /*usage_scenario_data_store=*/nullptr));
}

// static
std::unique_ptr<ChromeResponsivenessCalculatorDelegate>
ChromeResponsivenessCalculatorDelegate::CreateForTesting(
    UsageScenarioDataStore* usage_scenario_data_store) {
  return base::WrapUnique(
      new ChromeResponsivenessCalculatorDelegate(usage_scenario_data_store));
}

ChromeResponsivenessCalculatorDelegate::
    ~ChromeResponsivenessCalculatorDelegate() = default;

void ChromeResponsivenessCalculatorDelegate::OnMeasurementIntervalEnded() {
  interval_scenario_params_ =
      GetLongIntervalScenario(usage_scenario_data_store_->ResetIntervalData());
}

void ChromeResponsivenessCalculatorDelegate::OnResponsivenessEmitted(
    int num_congested_slices,
    int min,
    int exclusive_max,
    size_t buckets) {
  CHECK(interval_scenario_params_);
  base::UmaHistogramCustomCounts(
      base::StrCat({"Browser.MainThreadsCongestion",
                    interval_scenario_params_->histogram_suffix}),
      num_congested_slices, min, exclusive_max, buckets);

  if (IsChromeUsedInScenario(interval_scenario_params_->scenario)) {
    base::UmaHistogramCustomCounts("Browser.MainThreadsCongestion.Used",
                                   num_congested_slices, min, exclusive_max,
                                   buckets);
  }
}

ChromeResponsivenessCalculatorDelegate::ChromeResponsivenessCalculatorDelegate(
    UsageScenarioDataStore* usage_scenario_data_store)
    : usage_scenario_data_store_(usage_scenario_data_store) {
  if (!usage_scenario_data_store_) {
    usage_scenario_tracker_ = std::make_unique<UsageScenarioTracker>();
    usage_scenario_data_store_ = usage_scenario_tracker_->data_store();
  }
}
