// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dev_mode/dev_mode_policy_util.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

bool GetDeviceBlockDevModePolicyValue(
    const em::PolicyFetchResponse& policy_fetch_response) {
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy_fetch_response.policy_data())) {
    LOG(ERROR) << "Failed to parse policy data";
    return false;
  }

  em::ChromeDeviceSettingsProto payload;
  if (!payload.ParseFromString(policy_data.policy_value())) {
    LOG(ERROR) << "Failed to parse policy value";
    return false;
  }

  return GetDeviceBlockDevModePolicyValue(payload);
}

bool GetDeviceBlockDevModePolicyValue(
    const em::ChromeDeviceSettingsProto& device_policy) {
  bool block_devmode = false;
  if (device_policy.has_system_settings()) {
    const em::SystemSettingsProto& container = device_policy.system_settings();
    if (container.has_block_devmode()) {
      block_devmode = container.block_devmode();
    }
  }

  return block_devmode;
}

bool IsDeviceBlockDevModePolicyAllowed() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisallowPolicyBlockDevMode)) {
    base::SysInfo::CrashIfChromeOSNonTestImage();
    return false;
  }
  return true;
}

}  // namespace policy
