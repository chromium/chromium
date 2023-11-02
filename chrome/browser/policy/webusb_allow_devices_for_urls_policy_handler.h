// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_WEBUSB_ALLOW_DEVICES_FOR_URLS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_WEBUSB_ALLOW_DEVICES_FOR_URLS_POLICY_HANDLER_H_

#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the WebUsbAllowDevicesForUrls policy.
class WebUsbAllowDevicesForUrlsPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit WebUsbAllowDevicesForUrlsPolicyHandler(const Schema& schema);
  WebUsbAllowDevicesForUrlsPolicyHandler(
      const WebUsbAllowDevicesForUrlsPolicyHandler&) = delete;
  WebUsbAllowDevicesForUrlsPolicyHandler& operator=(
      const WebUsbAllowDevicesForUrlsPolicyHandler&) = delete;
  ~WebUsbAllowDevicesForUrlsPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* error) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_WEBUSB_ALLOW_DEVICES_FOR_URLS_POLICY_HANDLER_H_
