// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_HELPER_H_
#define CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_HELPER_H_

#include <string>

namespace heavy_ads {

// Returns a string containing HTML of an error page for the heavy ad
// intervention.
std::string PrepareHeavyAdPage();

}  // namespace heavy_ads

#endif  // CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_HELPER_H_
