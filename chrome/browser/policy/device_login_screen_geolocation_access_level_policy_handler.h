// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_LOGIN_SCREEN_GEOLOCATION_ACCESS_LEVEL_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEVICE_LOGIN_SCREEN_GEOLOCATION_ACCESS_LEVEL_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
class PrefValueMap;

namespace policy {

class PolicyMap;

class DeviceLoginScreenGeolocationAccessLevelPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  DeviceLoginScreenGeolocationAccessLevelPolicyHandler();
  DeviceLoginScreenGeolocationAccessLevelPolicyHandler(
      const DeviceLoginScreenGeolocationAccessLevelPolicyHandler&) = delete;

  DeviceLoginScreenGeolocationAccessLevelPolicyHandler& operator=(
      const DeviceLoginScreenGeolocationAccessLevelPolicyHandler&) = delete;

  ~DeviceLoginScreenGeolocationAccessLevelPolicyHandler() override;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The minimum value allowed.
  static constexpr int min_ = 0;

  // The maximum value allowed.
  static constexpr int max_ = static_cast<int>(
      enterprise_management::DeviceLoginScreenGeolocationAccessLevelProto::
          GeolocationAccessLevel_MAX);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_LOGIN_SCREEN_GEOLOCATION_ACCESS_LEVEL_POLICY_HANDLER_H_
