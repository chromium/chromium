// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/chrome_responsiveness_calculator_delegate.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
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
      NOTREACHED();
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// Returns true if available memory is less than 5.7% of total memory. It's
// based on ChromeOS stable 7 day aggregation ending May 20th 2024 from
// Memory.Experimental.AvailableMemoryPercent 10 percentile.
bool IsLowMemory() {
  auto available_bytes = base::SysInfo::AmountOfAvailablePhysicalMemory();
  auto total_bytes = base::SysInfo::AmountOfPhysicalMemory();
  return (available_bytes * 1000 / total_bytes) < 57;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

const char* GetSuffixForExtensionCount(size_t extension_count) {
  if (extension_count >= 16) {
    return "16";
  }
  if (extension_count >= 8) {
    return "8";
  }
  if (extension_count >= 4) {
    return "4";
  }
  if (extension_count >= 2) {
    return "2";
  }
  if (extension_count == 1) {
    return "1";
  }
  return "0";
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
  auto interval_data = usage_scenario_data_store_->ResetIntervalData();
  interval_scenario_params_ = GetLongIntervalScenario(interval_data);
  extensions_with_content_scripts_in_interval_ =
      interval_data.num_extensions_with_content_scripts;
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
  if (extensions_with_content_scripts_in_interval_.has_value()) {
    base::UmaHistogramCustomCounts(
        base::StrCat(
            {"Browser.MainThreadsCongestion.ExtensionContentScripts.",
             GetSuffixForExtensionCount(
                 extensions_with_content_scripts_in_interval_.value())}),
        num_congested_slices, min, exclusive_max, buckets);
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (IsLowMemory()) {
    base::UmaHistogramCustomCounts("Browser.MainThreadsCongestion.LowMemory",
                                   num_congested_slices, min, exclusive_max,
                                   buckets);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

ChromeResponsivenessCalculatorDelegate::ChromeResponsivenessCalculatorDelegate(
    UsageScenarioDataStore* usage_scenario_data_store)
    : usage_scenario_data_store_(usage_scenario_data_store) {
  if (!usage_scenario_data_store_) {
    usage_scenario_tracker_ = std::make_unique<UsageScenarioTracker>();
    usage_scenario_data_store_ = usage_scenario_tracker_->data_store();
  }
}
