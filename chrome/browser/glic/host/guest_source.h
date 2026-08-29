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

// Returns the full JavaScript source string for `glicGuestAPISource` to be
// injected into guest frames. This includes the prepended loadTimeData
// configuration header followed by the injected client Rollup JS bundle.
std::string GetGuestAPISource();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_SOURCE_H_
