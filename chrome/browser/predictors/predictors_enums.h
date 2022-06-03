// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTORS_ENUMS_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTORS_ENUMS_H_

namespace predictors {

// Enumerates the states for when optimization hints were received by the
// loading predictor if predictions were requested from the Optimization Guide
// for the navigation.
// This should be kept in sync with
// LoadingPredictorOptimizationHintsReceiveStatus in enums.xml.
enum class OptimizationHintsReceiveStatus {
  kUnknown,
  kBeforeNavigationFinish,
  kAfterNavigationFinish,
  kAfterRedirectOrNextNavigationStart,

  // Add new values above this line.
  kMaxValue = kAfterRedirectOrNextNavigationStart,
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTORS_ENUMS_H_
