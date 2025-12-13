// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_config.h"

#include "base/feature_list.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "chrome/common/chrome_features.h"
#include "components/subresource_filter/core/common/common_features.h"

namespace {

}  // namespace

OomInterventionConfig::OomInterventionConfig()
    : is_intervention_enabled_(
          base::SysInfo::IsLowEndDevice() &&
          base::FeatureList::IsEnabled(subresource_filter::kAdTagging) &&
          base::FeatureList::IsEnabled(features::kOomIntervention)),
      renderer_detection_args_(blink::mojom::DetectionArgs::New()) {
  if (!is_intervention_enabled_)
    return;

  is_renderer_pause_enabled_ = true;
  is_navigate_ads_enabled_ = true;
  is_purge_v8_memory_enabled_ = true;
  should_detect_in_renderer_ = true;

  use_components_callback_ = true;

  // If no threshold is specified, set blink_workload_threshold to 10% of the
  // RAM size.
  renderer_detection_args_->private_footprint_threshold =
      base::SysInfo::AmountOfPhysicalMemory().InBytesUnsigned() * 0.14;
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
