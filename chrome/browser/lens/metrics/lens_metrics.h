// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_METRICS_LENS_METRICS_H_
#define CHROME_BROWSER_LENS_METRICS_LENS_METRICS_H_

namespace lens {

constexpr char kLensRegionSearchCaptureResultHistogramName[] =
    "Search.RegionsSearch.Lens.Result";

// This should be kept in sync with the LensRegionSearchCaptureResult enum in
// tools/metrics/histograms/enums.xml.
enum class LensRegionSearchCaptureResult {
  SUCCESS = 0,
  FAILED_TO_OPEN_TAB = 1,
  ERROR_CAPTURING_REGION = 2,
  kMaxValue = ERROR_CAPTURING_REGION
};

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_METRICS_LENS_METRICS_H_
