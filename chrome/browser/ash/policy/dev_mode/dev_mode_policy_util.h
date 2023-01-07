// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DEV_MODE_DEV_MODE_POLICY_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_DEV_MODE_DEV_MODE_POLICY_UTIL_H_

namespace enterprise_management {
class ChromeDeviceSettingsProto;
class PolicyFetchResponse;
}  // namespace enterprise_management

namespace policy {

// Interprets |policy_fetch_response| as a device policy fetch response.
// Returns true if the 'DeviceBlockDevmode' policy is set to true in the parsed
// device policy.
// Returns false if any embedded layer (PolicyData / ChromeDeviceSettingsProto)
// could not be parsed.
bool GetDeviceBlockDevModePolicyValue(
    const enterprise_management::PolicyFetchResponse& policy_fetch_response);

// Returns true if the 'DeviceBlockDevmode' policy is set to true in the passed
// device policy.
bool GetDeviceBlockDevModePolicyValue(
    const enterprise_management::ChromeDeviceSettingsProto& device_policy);

// If this returns true, blocking developer mode by enterprise device policy
// should be disallowed.
// - Fail enterprise enrollment if enrolling would block dev mode.
// - Don't apply new device policy if it would block dev mode.
// Can only return false on Chrome OS test images.
bool IsDeviceBlockDevModePolicyAllowed();

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DEV_MODE_DEV_MODE_POLICY_UTIL_H_
