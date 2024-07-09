// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

namespace ash {

// Please keep in sync with the `Ash.MagicBoost.DisclaimerView.{OptInFeatures}`
// histogram name found in
// //tools/metrics/histograms/metadata/ash/histograms.xml.
void RecordDisclaimerViewActionMetrics(OptInFeatures opt_in_features,
                                       DisclaimerViewAction action) {
  std::string histogram_name = kMagicBoostDisclaimerViewHistogram;
  auto total_histogram_name = histogram_name + "Total";
  switch (opt_in_features) {
    case OptInFeatures::kHmrOnly:
      histogram_name += "HmrOnly";
      break;
    case OptInFeatures::kOrcaAndHmr:
      histogram_name += "OrcaAndHmr";
      break;
  }

  base::UmaHistogramEnumeration(histogram_name, action);
  base::UmaHistogramEnumeration(total_histogram_name, action);
}

}  // namespace ash
