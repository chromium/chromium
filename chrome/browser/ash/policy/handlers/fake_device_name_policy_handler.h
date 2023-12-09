// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_FAKE_DEVICE_NAME_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_FAKE_DEVICE_NAME_POLICY_HANDLER_H_

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"

namespace policy {

// Fake DeviceNamePolicyHandler implementation
class FakeDeviceNamePolicyHandler : public DeviceNamePolicyHandler {
 public:
  // If the device is managed, the initial device name policy should be
  // kPolicyHostnameNotConfigurable instead of the default kNoPolicy that is set
  // for unmanaged devices.
  explicit FakeDeviceNamePolicyHandler(
      DeviceNamePolicy initial_policy = DeviceNamePolicy::kNoPolicy);
  ~FakeDeviceNamePolicyHandler() override;

  // DeviceNamePolicyHandler:
  DeviceNamePolicy GetDeviceNamePolicy() const override;
  std::optional<std::string> GetHostnameChosenByAdministrator() const override;

  // Sets new device name and policy if different from the current device name
  // and/or policy.
  void SetPolicyState(DeviceNamePolicy policy,
                      const std::optional<std::string>& hostname_from_template);

 private:
  std::optional<std::string> hostname_ = std::nullopt;
  DeviceNamePolicy device_name_policy_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_FAKE_DEVICE_NAME_POLICY_HANDLER_H_
