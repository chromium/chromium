// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_TEST_UTILS_H_

#include <string>

#include "base/values.h"

namespace policy {

namespace dlp_test_util {
// For testing purposes, the following functions are used for creating the value
// object of DataLeakPreventionRulesList policy.

base::Value CreateSources(base::Value urls);

base::Value CreateDestinations(base::Value urls, base::Value components);

base::Value CreateRestrictionWithLevel(const std::string& restriction,
                                       const std::string& level);

base::Value CreateRule(const std::string& name,
                       const std::string& desc,
                       base::Value src_urls,
                       absl::optional<base::Value> dst_urls,
                       absl::optional<base::Value> dst_components,
                       base::Value restrictions);

}  // namespace dlp_test_util

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_TEST_UTILS_H_
