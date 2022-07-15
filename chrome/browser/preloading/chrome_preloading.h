// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_
#define CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_

#include "content/public/browser/preloading.h"

// Defines various embedder triggering mechanisms which triggers different
// preloading operations mentioned in //content/public/browser/preloading.h.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromePreloadingPredictor {
  // Numbering starts from `kPreloadingPredictorContentEnd` defined in
  // //content/browser/public/preloading.h . Advance numbering by +1 when adding
  // a new element.

  // When the preloading URL is predicted from the Omnibox Direct URL Input
  // (DUI). This is used to perform various preloading operations like prefetch
  // and prerender to load Omnibox predicted URLs faster.
  kOmniboxDirectURLInput = static_cast<int>(
      content::PreloadingPredictor::kPreloadingPredictorContentEnd),

  // When a pointerdown (e.g. mousedown or touchstart) event happens on an
  // anchor element with an href value pointing to an HTTP(S) origin, we may
  // attempt to preload the link.
  kPointerDownOnAnchor =
      static_cast<int>(
          content::PreloadingPredictor::kPreloadingPredictorContentEnd) +
      1,

  // TODO(crbug.com/1309934): Integrate more Preloading predictors with
  // Preloading logging APIs.
};

// Helper method to convert ChromePreloadingPredictor to
// content::PreloadingPredictor to avoid casting.
content::PreloadingPredictor ToPreloadingPredictor(
    ChromePreloadingPredictor predictor);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromePreloadingEligibility {
  // Numbering starts from `kPreloadingEligibilityContentEnd` defined in
  // //content/public/preloading.h . Advance numbering by +1 when adding a new
  // element.

  // Chrome was unable to get a LoadingPredictor object for the user profile.
  kUnableToGetLoadingPredictor = static_cast<int>(
      content::PreloadingEligibility::kPreloadingEligibilityContentEnd),
};

// Helper method to convert ChromePreloadingEligibility to
// content::PreloadingEligibility to avoid casting.
content::PreloadingEligibility ToPreloadingEligibility(
    ChromePreloadingEligibility eligibility);

#endif  // CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_
