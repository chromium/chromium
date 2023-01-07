// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_validator.h"

#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

template class CloudPolicyValidator<em::ChromeDeviceSettingsProto>;

}  // namespace policy
