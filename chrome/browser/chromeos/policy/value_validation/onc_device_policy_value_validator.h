// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_VALUE_VALIDATION_ONC_DEVICE_POLICY_VALUE_VALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_VALUE_VALIDATION_ONC_DEVICE_POLICY_VALUE_VALIDATOR_H_

#include "chrome/browser/chromeos/policy/value_validation/onc_policy_value_validator_base.h"

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}

namespace policy {

class ONCDevicePolicyValueValidator
    : public ONCPolicyValueValidatorBase<
          enterprise_management::ChromeDeviceSettingsProto> {
 public:
  ONCDevicePolicyValueValidator();

 protected:
  // ONCPolicyValueValidatorBase:
  absl::optional<std::string> GetONCStringFromPayload(
      const enterprise_management::ChromeDeviceSettingsProto& policy_payload)
      const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ONCDevicePolicyValueValidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_VALUE_VALIDATION_ONC_DEVICE_POLICY_VALUE_VALIDATOR_H_
