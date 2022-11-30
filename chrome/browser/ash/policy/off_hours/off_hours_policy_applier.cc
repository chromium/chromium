// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/off_hours/off_hours_policy_applier.h"

#include "chrome/browser/ash/policy/core/device_policy_remover.h"
#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"

namespace em = enterprise_management;

namespace policy {
namespace off_hours {

std::unique_ptr<em::ChromeDeviceSettingsProto> ApplyOffHoursPolicyToProto(
    const em::ChromeDeviceSettingsProto& input_policies) {
  if (!input_policies.has_device_off_hours())
    return nullptr;
  const em::DeviceOffHoursProto& container(input_policies.device_off_hours());
  std::vector<int> ignored_policy_proto_tags =
      ExtractIgnoredPolicyProtoTagsFromProto(container);
  std::unique_ptr<em::ChromeDeviceSettingsProto> policies =
      std::make_unique<em::ChromeDeviceSettingsProto>(input_policies);
  RemovePolicies(policies.get(), ignored_policy_proto_tags);
  return policies;
}

}  // namespace off_hours
}  // namespace policy
