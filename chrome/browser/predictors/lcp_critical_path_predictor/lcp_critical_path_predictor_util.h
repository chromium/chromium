// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_

#include "chrome/browser/predictors/resource_prefetch_predictor.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

namespace predictors {

// Converts LcppData to LCPCriticalPathPredictorNavigationTimeHint
// so that it can be passed to the renderer via the navigation handle.
absl::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppDataToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppData& data);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
