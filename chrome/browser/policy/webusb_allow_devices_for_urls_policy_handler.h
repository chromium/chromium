// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_WEBUSB_ALLOW_DEVICES_FOR_URLS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_WEBUSB_ALLOW_DEVICES_FOR_URLS_POLICY_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefRegistrySimple;
class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the WebUsbAllowDevicesForUrls policy.
class WebUsbAllowDevicesForUrlsPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  static std::unique_ptr<WebUsbAllowDevicesForUrlsPolicyHandler>
  CreateForUserPolicy(const Schema& chrome_schema);

#if defined(OS_CHROMEOS)
  static std::unique_ptr<WebUsbAllowDevicesForUrlsPolicyHandler>
  CreateForDevicePolicy(const Schema& chrome_schema);

  static void RegisterPrefs(PrefRegistrySimple* registry);
#endif  // defined(OS_CHROMEOS)

  WebUsbAllowDevicesForUrlsPolicyHandler(const char* policy_name,
                                         const char* pref_name,
                                         const Schema& chrome_schema);
  ~WebUsbAllowDevicesForUrlsPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* error) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The name of the pref to apply the policy to.
  const char* pref_name_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbAllowDevicesForUrlsPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_WEBUSB_ALLOW_DEVICES_FOR_URLS_POLICY_HANDLER_H_
