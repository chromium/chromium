// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_FEATURE_FLAG_HELPER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_FEATURE_FLAG_HELPER_H_

namespace optimization_guide {

bool ShouldUseOptimizationGuideForDelayAsyncScript();

bool ShouldUseOptimizationGuideForDelayCompetingLowPriorityRequests();

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_FEATURE_FLAG_HELPER_H_
