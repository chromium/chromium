// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_HATS_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_HATS_UTIL_H_

#include <optional>
#include <string>

#include "components/signin/public/base/signin_metrics.h"

class Profile;

namespace signin {
// Returns true if the HaTS survey associated with the given `trigger` is
// enabled via its corresponding feature flag.
bool IsFeatureEnabledForSigninHatsTrigger(const std::string& trigger);

// Launches a HaTS survey for `profile`.
// On Win/Mac/Linux, if no browser is active for the profile and
// `defer_if_no_browser` is true, the survey is deferred until a browser becomes
// available. Otherwise, this is a no-op.
void LaunchSigninHatsSurveyForProfile(const std::string& trigger,
                                      Profile* profile,
                                      bool defer_if_no_browser = false,
                                      std::optional<signin_metrics::AccessPoint>
                                          access_point_for_data_type_promo =
                                              std::nullopt);
}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_HATS_UTIL_H_
