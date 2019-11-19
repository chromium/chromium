// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_POLICY_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_POLICY_INFO_H_

#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace base {
class Value;
}

namespace enterprise_reporting {

void AppendChromePolicyInfoIntoProfileReport(
    const base::Value& policies,
    em::ChromeUserProfileInfo* profile_info);

void AppendExtensionPolicyInfoIntoProfileReport(
    const base::Value& policies,
    em::ChromeUserProfileInfo* profile_info);

void AppendMachineLevelUserCloudPolicyFetchTimestamp(
    em::ChromeUserProfileInfo* profile_info);

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_POLICY_INFO_H_
