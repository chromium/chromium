// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_config.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/common/chrome_features.h"
#include "components/subresource_filter/core/common/common_features.h"

namespace {

const char kUseComponentCallbacks[] = "use_component_callbacks";
const char kSwapFreeThresholdRatioParamName[] = "swap_free_threshold_ratio";
const char kRendererWorkloadThresholdDeprecated[] =
    "renderer_workload_threshold";

const char kRendererWorkloadThresholdPercentage[] =
    "renderer_workload_threshold_percentage";
const char kRendererPMFThresholdPercentage[] =
    "renderer_pmf_threshold_percentage";
const char kRendererSwapThresholdPercentage[] =
    "renderer_swap_threshold_percentage";
const char kRendererVirtualMemThresholdPercentage[] =
    "renderer_virtual_mem_threshold_percentage";

// Default SwapFree/SwapTotal ratio for detecting near-OOM situation.
// TODO(bashi): Confirm that this is appropriate.
const double kDefaultSwapFreeThresholdRatio = 0.45;

// Field trial parameter names.
const char kRendererPauseParamName[] = "pause_renderer";
const char kNavigateAdsParamName[] = "navigate_ads";
const char kPurgeV8MemoryParamName[] = "purge_v8";
const char kShouldDetectInRenderer[] = "detect_in_renderer";

bool GetThresholdParam(const char* param,
                       size_t ram_size,
                       uint64_t* threshold) {
  std::string str =
      base::GetFieldTrialParamValueByFeature(features::kOomIntervention, param);
  uint64_t value = 0;
  if (!base::StringToUint64(str, &value))
    return false;
  *threshold = value * ram_size / 100;
  return *threshold > 0;
}

bool GetRendererMemoryThresholds(blink::mojom::DetectionArgsPtr* args) {
  static size_t kRAMSize = base::SysInfo::AmountOfPhysicalMemory();

  bool has_blink =
      GetThresholdParam(kRendererWorkloadThresholdPercentage, kRAMSize,
                        &(*args)->blink_workload_threshold);
  bool has_pmf = GetThresholdParam(kRendererPMFThresholdPercentage, kRAMSize,
                                   &(*args)->private_footprint_threshold);
  bool has_swap = GetThresholdParam(kRendererSwapThresholdPercentage, kRAMSize,
                                    &(*args)->swap_threshold);
  bool has_vm_size = GetThresholdParam(kRendererVirtualMemThresholdPercentage,
                                       kRAMSize, &(*args)->swap_threshold);
  if (has_blink || has_pmf || has_swap || has_vm_size)
    return true;

  // Check for old trigger param. If the old trigger param is set, then enable
  // intervention only on 512MB devices.
  if (kRAMSize > 512 * 1024 * 1024)
    return false;
  std::string threshold_str = base::GetFieldTrialParamValueByFeature(
      features::kOomIntervention, kRendererWorkloadThresholdDeprecated);
  uint64_t threshold = 0;
  if (base::StringToUint64(threshold_str, &threshold) && threshold > 0) {
    (*args)->blink_workload_threshold = threshold;
    return true;
  }

  // If no param is set then the intervention is enabled. No default param is
  // assumed.
  return false;
}

bool GetSwapFreeThreshold(uint64_t* threshold) {
  base::SystemMemoryInfoKB memory_info;
  if (!base::GetSystemMemoryInfo(&memory_info))
    return false;

  // If there is no swap (zram) the monitor doesn't work because we use
  // SwapFree as the tracking metric.
  if (memory_info.swap_total == 0)
    return false;

  double threshold_ratio = base::GetFieldTrialParamByFeatureAsDouble(
      features::kOomIntervention, kSwapFreeThresholdRatioParamName,
      kDefaultSwapFreeThresholdRatio);
  *threshold = static_cast<uint64_t>(memory_info.swap_total * threshold_ratio);
  return true;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OomInterventionBrowserMonitorStatus {
  kEnabledWithValidConfig = 0,
  kDisabledWithInvalidParam = 1,
  kEnabledWithNoSwap = 2,
  kMaxValue = kEnabledWithNoSwap
};

}  // namespace

OomInterventionConfig::OomInterventionConfig()
    : is_intervention_enabled_(
          base::SysInfo::IsLowEndDevice() &&
          base::FeatureList::IsEnabled(subresource_filter::kAdTagging) &&
          base::FeatureList::IsEnabled(features::kOomIntervention)),
      renderer_detection_args_(blink::mojom::DetectionArgs::New()) {
  if (!is_intervention_enabled_)
    return;

  is_renderer_pause_enabled_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kRendererPauseParamName, true);
  is_navigate_ads_enabled_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kNavigateAdsParamName, true);
  is_purge_v8_memory_enabled_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kPurgeV8MemoryParamName, true);
  should_detect_in_renderer_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kShouldDetectInRenderer, true);

  use_components_callback_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kUseComponentCallbacks, true);

  OomInterventionBrowserMonitorStatus status =
      OomInterventionBrowserMonitorStatus::kEnabledWithValidConfig;
  if (!GetSwapFreeThreshold(&swapfree_threshold_)) {
    is_swap_monitor_enabled_ = false;
    status = OomInterventionBrowserMonitorStatus::kEnabledWithNoSwap;
  }
  // If no threshold is specified, set blink_workload_threshold to 10% of the
  // RAM size.
  if (!GetRendererMemoryThresholds(&renderer_detection_args_)) {
    renderer_detection_args_->private_footprint_threshold =
        base::SysInfo::AmountOfPhysicalMemory() * 0.14;
  }
}

// static
const OomInterventionConfig* OomInterventionConfig::GetInstance() {
  static OomInterventionConfig* config = new OomInterventionConfig();
  return config;
}

blink::mojom::DetectionArgsPtr
OomInterventionConfig::GetRendererOomDetectionArgs() const {
  return renderer_detection_args_.Clone();
}
