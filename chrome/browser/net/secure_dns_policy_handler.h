// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_SECURE_DNS_POLICY_HANDLER_H_

#include <string_view>

#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

// Handles DnsOverHttpsMode, DnsOverHttpsTemplates,
// DnsOverHttpsTemplatesWithIdentifiers and DnsOverHttpsSalt policies.
class SecureDnsPolicyHandler : public ConfigurationPolicyHandler {
 public:
  SecureDnsPolicyHandler();

  SecureDnsPolicyHandler(const SecureDnsPolicyHandler&) = delete;
  SecureDnsPolicyHandler& operator=(const SecureDnsPolicyHandler&) = delete;

  ~SecureDnsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Returns true if templates must be specified (i.e. `mode_str` is secure),
  // but they are not set or invalid (non-string).
  bool IsTemplatesPolicyNotSpecified(bool is_templates_policy_valid,
                                     std::string_view mode_str);
  // Indicates whether the DnsOverHttpsTemplates policy is valid and can be
  // applied. If not, the corresponding pref is not set. If the DNS mode is
  // secure, either `is_templates_policy_valid_` or, on Chrome OS only,
  // `is_templates_with_identifiers_policy_valid_` must be true, otherwise
  // `CheckPolicySettings` will report a policy error. Set in
  // `CheckPolicySettings`.
  bool is_templates_policy_valid_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Indicates whether the DnsOverHttpsTemplatesWithIdentifiers policy is valid
  // and can be applied. If not, the corresponding pref is not set. Set in
  // `CheckPolicySettings`.
  bool is_templates_with_identifiers_policy_valid_ = false;
#endif
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_SECURE_DNS_POLICY_HANDLER_H_
