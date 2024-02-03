// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_TEST_UTILS_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/authenticator.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/config_source.h"

namespace base {
class Value;
}

namespace ash {
namespace parent_access {

// Values used in default parent access code configuration for tests.
constexpr char kTestSharedSecret[] = "AIfVJHITSar8keeq3779V70dWiS1xbPv8g";
constexpr base::TimeDelta kDefaultCodeValidity = base::Minutes(10);
constexpr base::TimeDelta kDefaultClockDrift = base::Minutes(5);

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

// Returns a policy representing the configs that are passed in.
base::Value PolicyFromConfigs(const AccessCodeConfig& future_config,
                              const AccessCodeConfig& current_config,
                              const std::vector<AccessCodeConfig>& old_configs);

}  // namespace parent_access
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_TEST_UTILS_H_
