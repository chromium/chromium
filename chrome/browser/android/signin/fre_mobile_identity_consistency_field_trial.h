// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_FRE_MOBILE_IDENTITY_CONSISTENCY_FIELD_TRIAL_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_FRE_MOBILE_IDENTITY_CONSISTENCY_FIELD_TRIAL_H_

#include <string>

namespace fre_mobile_identity_consistency_field_trial {

// Returns the field trial group created in Java code.
// The groups are created in FREFieldTrial.java.
std::string GetFREFieldTrialGroup();

}  // namespace fre_mobile_identity_consistency_field_trial

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_FRE_MOBILE_IDENTITY_CONSISTENCY_FIELD_TRIAL_H_
