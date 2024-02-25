// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/fake_device_name_policy_handler.h"

namespace policy {

FakeDeviceNamePolicyHandler::FakeDeviceNamePolicyHandler(
    DeviceNamePolicy initial_policy)
    : device_name_policy_(initial_policy) {}

FakeDeviceNamePolicyHandler::~FakeDeviceNamePolicyHandler() = default;

DeviceNamePolicyHandler::DeviceNamePolicy
FakeDeviceNamePolicyHandler::GetDeviceNamePolicy() const {
  return device_name_policy_;
}

std::optional<std::string>
FakeDeviceNamePolicyHandler::GetHostnameChosenByAdministrator() const {
  return hostname_;
}

void FakeDeviceNamePolicyHandler::SetPolicyState(
    DeviceNamePolicy policy,
    const std::optional<std::string>& hostname_from_template) {
  // Hostname from template should only be relevant for
  // kPolicyHostnameChosenByAdmin policy, hence should be null for any other
  // policies.
  if (policy !=
      DeviceNamePolicyHandler::DeviceNamePolicy::kPolicyHostnameChosenByAdmin) {
    DCHECK(!hostname_from_template);
  }

  if (device_name_policy_ == policy && hostname_ == hostname_from_template)
    return;
  device_name_policy_ = policy;
  hostname_ = hostname_from_template;
  NotifyHostnamePolicyChanged();
}

}  // namespace policy
