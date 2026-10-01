// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_FEATURES_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Controls the lifetime of the DSE DeviceBoundSessionPrewarmer relative to the
// SearchEnginePreconnector.
enum class DsePrewarmerLifetime {
  // The prewarmer follows the preconnect cycle: it is created only by
  // successful foreground preconnect attempts, and it is destroyed whenever a
  // preconnect attempt is skipped or the preconnector is stopped.
  kFollowPreconnector,
  // The prewarmer ignores temporary conditions: it is created right after the
  // preconnector is started (without waiting for the startup delay or the app
  // being in the foreground), and it is kept when preconnecting is skipped or
  // the preconnector is stopped. The user settings (search suggestions,
  // preloading and network prediction prefs/policies) are respected at
  // creation time only. The prewarmer is rebuilt when the DSE URL changes to
  // another preconnect-eligible HTTPS engine; otherwise, the previous DSE's
  // prewarmer is kept.
  kStickyRespectingPrefs,
  // Same as `kStickyRespectingPrefs`, but the user settings are ignored at
  // creation time as well.
  kSticky,
};

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
BASE_DECLARE_FEATURE(kDeviceBoundSessionsDsePrewarmer);
BASE_DECLARE_FEATURE_PARAM(DsePrewarmerLifetime,
                           kDeviceBoundSessionsDsePrewarmerMode);
BASE_DECLARE_FEATURE(kNavigationPredictorPreconnectHoldback);

}  // namespace features

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_FEATURES_H_
