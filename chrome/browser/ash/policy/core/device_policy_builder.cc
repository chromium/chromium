// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_builder.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace em = enterprise_management;

namespace policy {

template <>
TypedPolicyBuilder<em::ChromeDeviceSettingsProto>::TypedPolicyBuilder()
    : payload_(new em::ChromeDeviceSettingsProto()) {
  policy_data().set_policy_type(dm_protocol::kChromeDevicePolicyType);
}

// Have the instantiation compiled into the module.
template class TypedPolicyBuilder<em::ChromeDeviceSettingsProto>;

}  // namespace policy
