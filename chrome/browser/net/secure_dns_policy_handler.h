// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_SECURE_DNS_POLICY_HANDLER_H_

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

// Handles DnsOverHttpsMode policy.
class SecureDnsPolicyHandler : public ConfigurationPolicyHandler {
 public:
  SecureDnsPolicyHandler();
  ~SecureDnsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool IsTemplatesPolicyNotSpecified(const base::Value* templates,
                                     const base::StringPiece mode_str);
  bool IsTemplatesPolicyInvalid(const base::StringPiece templates_str);

  bool ShouldSetTemplatesPref(const base::Value* templates);

  DISALLOW_COPY_AND_ASSIGN(SecureDnsPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_SECURE_DNS_POLICY_HANDLER_H_
