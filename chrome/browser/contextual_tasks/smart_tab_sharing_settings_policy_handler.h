// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SMART_TAB_SHARING_SETTINGS_POLICY_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SMART_TAB_SHARING_SETTINGS_POLICY_HANDLER_H_

#include <memory>

#include "components/policy/core/browser/configuration_policy_handler.h"  // nogncheck

namespace policy {

class GenAiDefaultSettingsPolicyHandler;
class PolicyErrorMap;
class PolicyMap;
class SimplePolicyHandler;

class SmartTabSharingSettingsPolicyHandler : public IntRangePolicyHandler {
 public:
  explicit SmartTabSharingSettingsPolicyHandler(
      std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
          gen_ai_default_settings_policy_handler);
  ~SmartTabSharingSettingsPolicyHandler() override;

 protected:
  // `IntRangePolicyHandler`:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

 private:
  const std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
      gen_ai_default_settings_policy_handler_;
  const std::unique_ptr<SimplePolicyHandler>
      search_content_sharing_settings_policy_handler_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SMART_TAB_SHARING_SETTINGS_POLICY_HANDLER_H_
