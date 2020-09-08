// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_feature_flag_helper.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace optimization_guide {
namespace features {

bool ShouldUseOptimizationGuideForDelayAsyncScript() {
  static const bool is_feature_enabled =
      base::FeatureList::IsEnabled(
          blink::features::kDelayAsyncScriptExecution) &&
      blink::features::kDelayAsyncScriptExecutionDelayParam.Get() ==
          blink::features::DelayAsyncScriptDelayType::kUseOptimizationGuide;
  return is_feature_enabled;
}

bool ShouldUseOptimizationGuideForDelayCompetingLowPriorityRequests() {
  static const bool is_feature_enabled =
      base::FeatureList::IsEnabled(
          blink::features::kDelayCompetingLowPriorityRequests) &&
      blink::features::kDelayCompetingLowPriorityRequestsDelayParam.Get() ==
          blink::features::DelayCompetingLowPriorityRequestsDelayType::
              kUseOptimizationGuide;
  return is_feature_enabled;
}

}  // namespace features
}  // namespace optimization_guide
