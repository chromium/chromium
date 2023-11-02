// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_VALIDATOR_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_VALIDATOR_H_

#include "components/policy/core/common/cloud/cloud_policy_validator.h"

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}

namespace policy {

typedef CloudPolicyValidator<enterprise_management::ChromeDeviceSettingsProto>
    DeviceCloudPolicyValidator;

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_VALIDATOR_H_
