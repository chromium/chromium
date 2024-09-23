// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_WEBHID_DEVICE_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_WEBHID_DEVICE_POLICY_HANDLER_H_

#include <string_view>

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the WebHidAllowDevicesForUrls,
// DeviceLoginScreenWebHidAllowDevicesForUrls, and
// WebHidAllowDevicesWithHidUsagesForUrls policies.
class WebHidDevicePolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit WebHidDevicePolicyHandler(const char* policy_key,
                                     std::string_view pref_name,
                                     const Schema& schema);
  WebHidDevicePolicyHandler(const WebHidDevicePolicyHandler&) = delete;
  WebHidDevicePolicyHandler& operator=(const WebHidDevicePolicyHandler&) =
      delete;
  ~WebHidDevicePolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* error) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const std::string pref_name_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_WEBHID_DEVICE_POLICY_HANDLER_H_
