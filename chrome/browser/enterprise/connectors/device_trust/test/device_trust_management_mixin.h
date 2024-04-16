// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_MANAGEMENT_MIXIN_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_MANAGEMENT_MIXIN_H_

#include "chrome/browser/enterprise/connectors/device_trust/test/test_constants.h"
#include "chrome/browser/enterprise/connectors/test/management_context_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace enterprise_connectors::test {

struct DeviceTrustManagementLevel {
  bool is_managed = false;
  bool is_inline_policy_enabled = false;
};

struct DeviceTrustConnectorState {
  bool affiliated = false;
  bool consent_given = false;
  bool permanent_consent_given = false;

  DeviceTrustManagementLevel cloud_user_management_level{};
  DeviceTrustManagementLevel cloud_machine_management_level{};
};

// Utility class that can be used in Device Trust browser tests to simplify
// testing of various management state permutations (e.g. managed and policy
// enabled or not).
class DeviceTrustManagementMixin : public InProcessBrowserTestMixin {
 public:
  DeviceTrustManagementMixin(InProcessBrowserTestMixinHost* host,
                             InProcessBrowserTest* test_base,
                             DeviceTrustConnectorState device_trust_state);

  DeviceTrustManagementMixin(const DeviceTrustManagementMixin&) = delete;
  DeviceTrustManagementMixin& operator=(const DeviceTrustManagementMixin&) =
      delete;

  ~DeviceTrustManagementMixin() override;

  // Will start managing the current user, but won't enable DTC.
  void ManageCloudUser();

  // Will enable the cloud-machine inline flow policy by setting the policy
  // to the `url` which by default is [kAllowedHost].
  // On ChromeOS, this effectively only enables the login-screen policy.
  void EnableMachineInlinePolicy(const std::string& url = kAllowedHost);

  // Will disable the cloud-machine inline flow policy by setting the policy
  // value to an empty list.
  // On ChromeOS, this effectively only disables the login-screen policy.
  void DisableMachineInlinePolicy();

  // Will enable the cloud-user inline flow policy by setting the policy value
  // to the `url` which by default is [kAllowedHost].
  void EnableUserInlinePolicy(const std::string& url = kAllowedHost);

  // Will disable the cloud-user inline flow policy by setting the policy value
  // to an empty list.
  void DisableUserInlinePolicy();

  // Will disable all inline flow policies, effectively turning the feature off.
  void DisableAllInlinePolicies();

  // Update the user consent based on `consent_given`.
  void SetConsentGiven(bool consent_given);
  void SetPermanentConsentGiven(bool permanent_consent_given);

 protected:
  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  void SetMachineInlinePolicy(base::Value policy_value);
  void SetUserInlinePolicy(base::Value policy_value);

 private:
  const raw_ptr<InProcessBrowserTest> test_base_;
  DeviceTrustConnectorState device_trust_state_;
  std::unique_ptr<ManagementContextMixin> management_context_mixin_;
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_MANAGEMENT_MIXIN_H_
