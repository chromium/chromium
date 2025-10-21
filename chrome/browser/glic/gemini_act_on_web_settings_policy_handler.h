// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ACT_ON_WEB_SETTINGS_POLICY_HANDLER_H_
#define CHROME_BROWSER_GLIC_GEMINI_ACT_ON_WEB_SETTINGS_POLICY_HANDLER_H_

#include <memory>

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class GenAiDefaultSettingsPolicyHandler;
class PolicyErrorMap;
class PolicyMap;
class SimplePolicyHandler;

class GeminiActOnWebSettingsPolicyHandler : public IntRangePolicyHandler {
 public:
  explicit GeminiActOnWebSettingsPolicyHandler(
      std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
          gen_ai_default_settings_policy_handler);
  ~GeminiActOnWebSettingsPolicyHandler() override;

 protected:
  // `IntRangePolicyHandler`:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

 private:
  const std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
      gen_ai_default_settings_policy_handler_;
  const std::unique_ptr<SimplePolicyHandler> gemini_settings_policy_handler_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_GLIC_GEMINI_ACT_ON_WEB_SETTINGS_POLICY_HANDLER_H_
