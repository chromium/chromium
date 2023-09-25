// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_

#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

namespace predictors {

// Converts LcppData to LCPCriticalPathPredictorNavigationTimeHint
// so that it can be passed to the renderer via the navigation handle.
absl::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppDataToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppData& data);

// An input to update LcppData.
struct LcppDataInputs {
  LcppDataInputs();
  ~LcppDataInputs();

  // LCPP write path [1]: Staging area of the proto3 serialized element locator
  // of the latest LCP candidate element. [1]
  // https://docs.google.com/document/d/1waakt6bSvedWdaUQ2mC255NF4k8j7LybK2dQ7WptxiE/edit#heading=h.hy4g58pyf548
  // We do not know the final data to be serialized from the beginning.
  // They are updated on each LCP candidate. We record the data we had at the
  // `FinalizeLCP` timing as the representative, since they should have the
  // data of the LCP candidate that won.
  //
  // a locator of the LCP element.
  std::string lcp_element_locator;
  // async script urls of the latest LCP candidate element.
  std::vector<GURL> lcp_influencer_scripts;

  // Fetched font URLs.
  // Unlike data above, the field will be updated per font fetch.
  std::vector<GURL> font_urls;
};

bool UpdateLcppDataWithLcppDataInputs(const LoadingPredictorConfig& config,
                                      const LcppDataInputs& inputs,
                                      LcppData& data);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
