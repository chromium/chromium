// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_SYNTHETIC_TRIAL_CONSTANTS_H_
#define CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_SYNTHETIC_TRIAL_CONSTANTS_H_

namespace glic {

// Synthetic trial name for tracking referral acquisition source.
inline constexpr char kGlicPromotionSourceTrialName[] =
    "GlicPromotionSourceSynthetic";

// Group names for the GlicPromotionSourceSynthetic trial.
inline constexpr char kGlicPromotionSourceNonZss[] = "NonZss";
inline constexpr char kGlicPromotionSourceZss[] = "zss";
inline constexpr char kGlicPromotionSourceMultiple[] = "Multiple";
inline constexpr char kGlicPromotionSourceMultiProfileDetected[] =
    "MultiProfileDetected";

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_SYNTHETIC_TRIAL_CONSTANTS_H_
