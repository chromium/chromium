// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_FEATURES_H_
#define CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_FEATURES_H_

// Param that enabled heavy ad intervention with reporting only, does not
// unloaded the ads.
extern const char kHeavyAdReportingOnlyParamName[];

// Param that enabled sending intervention reports for frames unloaded by heavy
// ad intervention.
extern const char kHeavyAdReportingEnabledParamName[];

#endif  // CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_FEATURES_H_
