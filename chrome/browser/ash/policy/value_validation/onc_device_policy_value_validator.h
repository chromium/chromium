// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_VALUE_VALIDATION_ONC_DEVICE_POLICY_VALUE_VALIDATOR_H_
#define CHROME_BROWSER_ASH_POLICY_VALUE_VALIDATION_ONC_DEVICE_POLICY_VALUE_VALIDATOR_H_

#include "chrome/browser/ash/policy/value_validation/onc_policy_value_validator_base.h"

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}

namespace policy {

class ONCDevicePolicyValueValidator
    : public ONCPolicyValueValidatorBase<
          enterprise_management::ChromeDeviceSettingsProto> {
 public:
  ONCDevicePolicyValueValidator();

  ONCDevicePolicyValueValidator(const ONCDevicePolicyValueValidator&) = delete;
  ONCDevicePolicyValueValidator& operator=(
      const ONCDevicePolicyValueValidator&) = delete;

 protected:
  // ONCPolicyValueValidatorBase:
  std::optional<std::string> GetONCStringFromPayload(
      const enterprise_management::ChromeDeviceSettingsProto& policy_payload)
      const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_VALUE_VALIDATION_ONC_DEVICE_POLICY_VALUE_VALIDATOR_H_
