// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GUEST_SOURCE_H_
#define CHROME_BROWSER_GLIC_HOST_GUEST_SOURCE_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace glic {

BASE_DECLARE_FEATURE(kGlicMaxInFlightRequests);
extern const base::FeatureParam<int> kGlicMaxInFlightRequestLimit;
BASE_DECLARE_FEATURE(kGlicSendResponsesForAllRequests);

// Path suffix of the JS resource intercepted on the guest origin to serve the
// Glic API bundle. Any same-origin GET request from an allowed guest primary
// main frame whose URL path ends with `/chrome__glic_api.js` (e.g.,
// `/chrome__glic_api.js`, `/glic/chrome__glic_api.js`,
// `/u/1/chrome__glic_api.js`) is answered with `GetGuestAPISource()`.
inline constexpr char kGlicApiBundleResourcePathSuffix[] =
    "/chrome__glic_api.js";

// Returns the full JavaScript source string for `glicGuestAPISource` to be
// injected into guest frames. This includes the prepended loadTimeData
// configuration header followed by the injected client Rollup JS bundle.
std::string GetGuestAPISource();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_SOURCE_H_
