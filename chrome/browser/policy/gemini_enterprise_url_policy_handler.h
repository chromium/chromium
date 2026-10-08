// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_GEMINI_ENTERPRISE_URL_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_GEMINI_ENTERPRISE_URL_POLICY_HANDLER_H_

#include <string>

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"

namespace policy {

// ConfigurationPolicyHandler for the GeminiEnterpriseUrl policy.
class GeminiEnterpriseUrlPolicyHandler : public SimplePolicyHandler {
 public:
  GeminiEnterpriseUrlPolicyHandler();
  GeminiEnterpriseUrlPolicyHandler(const GeminiEnterpriseUrlPolicyHandler&) =
      delete;
  GeminiEnterpriseUrlPolicyHandler& operator=(
      const GeminiEnterpriseUrlPolicyHandler&) = delete;
  ~GeminiEnterpriseUrlPolicyHandler() override;

  // Checks `url` against the requirements the runtime imposes on the Gemini
  // Enterprise web application URL, adding an error for `policy_name` at
  // `error_path` on failure.
  static bool CheckUrl(const std::string& policy_name,
                       const std::string& url,
                       PolicyErrorMap* errors,
                       PolicyErrorPath error_path = {});

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_GEMINI_ENTERPRISE_URL_POLICY_HANDLER_H_
