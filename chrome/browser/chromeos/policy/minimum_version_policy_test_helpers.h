// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_TEST_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_TEST_HELPERS_H_

#include <string>

#include "base/values.h"

namespace policy {

// Creates and returns a dictionary value to represent minimum version
// requirement. |version| - a string containing the minimum required version.
// |warning| - number of days representing the warning period.
// |eol_warning| - number of days representing the end of life warning period.
base::Value CreateMinimumVersionPolicyRequirement(const std::string& version,
                                                  int warning,
                                                  int eol_warning);

// Creates and returns DeviceMinimumVersion policy value.
base::Value CreateMinimumVersionPolicyValue(base::Value requirements,
                                            bool unmanaged_user_restricted);

// Creates and returns DeviceMinimumVersion policy value with a single
// requirement entry.
base::Value CreateMinimumVersionSingleRequirementPolicyValue(
    const std::string& version,
    int warning,
    int eol_warning,
    bool unmanaged_user_restricted);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_MINIMUM_VERSION_POLICY_TEST_HELPERS_H_
