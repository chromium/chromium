// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_OFF_HOURS_OFF_HOURS_POLICY_APPLIER_H_
#define CHROME_BROWSER_ASH_POLICY_OFF_HOURS_OFF_HOURS_POLICY_APPLIER_H_

#include <memory>

#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {
namespace off_hours {

// Apply "OffHours" policy for proto which contains device policies. Return
// ChromeDeviceSettingsProto without policies from |ignored_policy_proto_tags|.
// The system will revert to the default behavior for the removed policies.
std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto>
ApplyOffHoursPolicyToProto(
    const enterprise_management::ChromeDeviceSettingsProto& input_policies);

}  // namespace off_hours
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_OFF_HOURS_OFF_HOURS_POLICY_APPLIER_H_
