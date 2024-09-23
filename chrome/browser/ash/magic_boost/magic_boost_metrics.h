// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_METRICS_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_METRICS_H_

#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

namespace ash {

inline constexpr char kMagicBoostDisclaimerViewHistogram[] =
    "Ash.MagicBoost.DisclaimerView.";

// Please keep in sync with the `DisclaimerViewAction` enum found in
// //tools/metrics/histograms/metadata/ash/enums.xml.
enum class DisclaimerViewAction {
  kShow = 0,
  kAcceptButtonPressed = 1,
  kDeclineButtonPressed = 2,
  kMaxValue = kDeclineButtonPressed,
};

// Records metrics for user interactions with the magic boost disclaimer view.
// This function tracks The specific action the user took on the view with the
// specific `OptInFeature` that triggered the view's display. What's more, it
// records the total number of times of actions using `OptInFeatures::kTotal`
// for overall tracking. For example:
//        Ash.MagicBoost.DisclaimerView.OrcaAndHmr -> DeclineButtonPressed
//    Indicates the disclaimer view was shown due to both Orca and HMR features,
//    and the user clicked the decline button.
//        Ash.MagicBoost.DisclaimerView.Total -> Show
//    Records a overall showing times of the disclaimer view.
void RecordDisclaimerViewActionMetrics(OptInFeatures opt_in_features,
                                       DisclaimerViewAction action);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_METRICS_H_
