// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_FRE_MOBILE_IDENTITY_CONSISTENCY_FIELD_TRIAL_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_FRE_MOBILE_IDENTITY_CONSISTENCY_FIELD_TRIAL_H_

#include <string>

#include "components/variations/variations_associated_data.h"

namespace fre_mobile_identity_consistency_field_trial {

// Returns the field trial group created in Java code.
// The groups are created in FREMobileIdentityConsistencyFieldTrial.java.
std::string GetFREFieldTrialGroup();

// Returns VariationID that should be associated with the selected group for
// FREMobileIdentityConsistencySynthetic. Returns variations::EMPTY_ID if no
// VariationID should be associated.
//
// `low_entropy_source` and `low_entropy_size` should contain the low entropy
// source used on C++ side. These values are used to check that the group
// assigned in the first run is still consistent with the current low entropy
// source. If the group is inconsistent (for example, if the low entropy source
// was reset) - this method will return variations::EMPTY_ID.
variations::VariationID GetFREFieldTrialVariationId(int low_entropy_source,
                                                    int low_entropy_size);

}  // namespace fre_mobile_identity_consistency_field_trial

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_FRE_MOBILE_IDENTITY_CONSISTENCY_FIELD_TRIAL_H_
