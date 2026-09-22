// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_GEMINI_ENTERPRISE_SETTINGS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_GEMINI_ENTERPRISE_SETTINGS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the GeminiEnterpriseSettings policy.
class GeminiEnterpriseSettingsPolicyHandler
    : public SimpleSchemaValidatingPolicyHandler {
 public:
  explicit GeminiEnterpriseSettingsPolicyHandler(const Schema& chrome_schema);
  GeminiEnterpriseSettingsPolicyHandler(
      const GeminiEnterpriseSettingsPolicyHandler&) = delete;
  GeminiEnterpriseSettingsPolicyHandler& operator=(
      const GeminiEnterpriseSettingsPolicyHandler&) = delete;
  ~GeminiEnterpriseSettingsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_GEMINI_ENTERPRISE_SETTINGS_POLICY_HANDLER_H_
