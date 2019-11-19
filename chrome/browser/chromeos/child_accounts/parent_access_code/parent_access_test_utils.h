// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_TEST_UTILS_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/authenticator.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/config_source.h"

namespace chromeos {
namespace parent_access {

// Values used in default parent access code configuration for tests.
constexpr char kTestSharedSecret[] = "AIfVJHITSar8keeq3779V70dWiS1xbPv8g";
constexpr base::TimeDelta kDefaultCodeValidity =
    base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kDefaultClockDrift = base::TimeDelta::FromMinutes(5);

// Used for storing sample parent access code data. Map that contains pairs of
// corresponding timestamp and code.
typedef std::map<base::Time, std::string> AccessCodeValues;

// Returns default configuration that is currently used for PAC tests.
// Sample test data were generated with this config.
AccessCodeConfig GetDefaultTestConfig();

// Returns configuration different that the default test config.
// Testing sample test date against this config will result with validation
// failures thus this config is referred to as 'invalid'.
AccessCodeConfig GetInvalidTestConfig();

// Populates |test_values| with test Parent Access Code data (timestamp - code
// value pairs) generated in Family Link Android app with the default config.
void GetTestAccessCodeValues(AccessCodeValues* test_values);

}  // namespace parent_access
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_TEST_UTILS_H_
