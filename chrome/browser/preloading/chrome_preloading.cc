// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/chrome_preloading.h"

content::PreloadingPredictor ToPreloadingPredictor(
    ChromePreloadingPredictor predictor) {
  return static_cast<content::PreloadingPredictor>(predictor);
}

content::PreloadingEligibility ToPreloadingEligibility(
    ChromePreloadingEligibility eligibility) {
  return static_cast<content::PreloadingEligibility>(eligibility);
}
